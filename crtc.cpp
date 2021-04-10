#include "mmu.h"
#include "sub.h"
#include "crtc.h"

byte CRTMode1,CRTMode2,CRTMode3;
byte CSS1,CSS2,CSS3;

byte UPeriod     = 2;               /* Interrupts/scr. update */
byte EndOfFrame=1;                  /* 1 when end of frame    */

byte TimerSW = 0;
byte TimerSW_F3 = 1;				/* 初期値は1とする（PC-6001対応） */


void LockSurface(void);
void UnlockSurface(void);

/** Screen Mode Handlers [N60/N66][SCREEN MODE] **************/
void (*SCR[2][4])() = 
{
  { RefreshScr10, RefreshScr10, RefreshScr10, RefreshScr10 },
  { RefreshScr51, RefreshScr51, RefreshScr53, RefreshScr54 },
};


byte *VRAM;
static unsigned int VRAMHead[2][4] = {
  { 0xc000, 0xe000, 0x8000, 0xa000 },
  { 0x8000, 0xc000, 0x0000, 0x4000 },
};


void DoOutB0(byte Port ,byte Value)
{
    switch(Port) {
		case 0xB0:
            VRAM=GetRAM()+VRAMHead[CRTMode1][(Value&0x06)>>1];
            TimerSW=(Value&0x01)?0:1;
            return;
    }
}


void DoOutC0(byte Port ,byte Value)
{
    switch(Port) {
		case 0xC0:                           /* CSS                  */
            CSS3=(Value&0x04)<<2;CSS2=(Value&0x02)<<2;CSS1=(Value&0x01)<<2;
            return;
    }
}

void DoOutC1(byte Port ,byte Value)
{
    switch(Port) {
		case 0xC1:                           /* CRT controller mode  */
        //    if((CRTMode1)&&(Value&0x02)) {ClearScr();}
            CRTMode1=(Value&0x02) ? 0 : 1;
            CRTMode2=(Value&0x04) ? 0 : 1;
            CRTMode3=(Value&0x08) ? 0 : 1;
            SetCGROM ((CRTMode1 == 0) ? GetCGROM1() : GetCGROM5());
            return;
    }
}


/* $B2hLL$N99?7=hM}(B */
/* Z80.c$B$+$i8F$P$l$k(B */
void UpdateScreen(void)
{
	static int count = 0;
	static int UCount=1;
	
	if( count++ == 60) {
		int pos;
		count =0;
		pos = GetCasPos();
		if( pos !=-1) {
		//	SetTitle( pos);
		}
	}
	if((!--UCount)||!EndOfFrame) {
		UCount=UPeriod;
		/* Refreshing screen: */
		LockSurface();              // SDL lock surface
		SCR[CRTMode1][CRTMode2 ? (CRTMode3 ? 3 : 2) : 0]();
		UnlockSurface();        // SDL Unlock surface
	}
}


/** iP6: PC-6000/6600 series emualtor ************************/
/**                                                         **/
/**                         Refresh.c                       **/
/**                                                         **/
/** by ISHIOKA Hiroshi 1998,1999                            **/
/** This code is based on fMSX written by Marat Fayzullin   **/
/** and Adaptions for any X-terminal by Arnold Metselaar    **/
/*************************************************************/

/* get Type for storing X-colors */
#include <string.h>
// #include <X11/Xlib.h>
#include "P6.h"
#include "Unix.h"
#define register /**/ 

extern int Width;

typedef struct  
{ 
  int bp_bitpix; 
  void (* SeqPix22Wide)  (ColTyp,ColTyp);
  void (* SeqPix22Narrow)(ColTyp,ColTyp); 
  void (* SeqPix21Wide)  (ColTyp); 
  void (* SeqPix21Narrow)(ColTyp); 
  void (* SeqPix41Wide)  (ColTyp); 
  void (* SeqPix41Narrow)(ColTyp); 
} bp_struct; 

/* pointers to pixelsetters, used to adapt to different screen depths
   and window widths: */
typedef void (* t_SeqPix22) (ColTyp,ColTyp);
typedef void (* t_SeqPix21) (ColTyp);
typedef void (* t_SeqPix41) (ColTyp);
static t_SeqPix22  SeqPix22;
static t_SeqPix21 SeqPix21;
static t_SeqPix41 SeqPix41;
static bp_struct *funcs;

/* variables used to handle the buffer, should be register static but
   this is hard to do */
static byte *P;
static byte buf;
static int sft;

/* initialisation of these variables */
static void SetScrVar(int y1,int x1)
{
  P=XBuf+(long)(Width*y1+x1)*scale*bitpix/8;
  buf=0;sft=(lsbfirst?0:8);
}

/* functions to set various numbers of pixels for various values of
   bitpix and lsbfirst. their names are built as follows:
   sp<md>_<bpp>[<bitfirst>]<width>
   <md>  =  22   to set 2 pixels in 2 colors or
            21   to set 2 pixels in 1 color
   <bpp> =  the number of bits per pixel
   <bitfirst> = m for msb first or
                l for lsb first
   <width>    = w for wide screen (set both pixels) or
                n for narrow screen (set only one)
*/

static void sp22_1mw(ColTyp c0,ColTyp c1)
{ buf+=(2*c0.ct_byte[0] +c1.ct_byte[0]) << (sft-=2);
  if (!sft) {*P++=buf; buf=0; sft=8;} }
static void sp22_1mn(ColTyp c0,ColTyp c1)
{ buf+= c0.ct_byte[0] << (--sft);
  if (!sft) {*P++=buf; buf=0; sft=8;} }
static void sp21_1mw(ColTyp c)
{ buf+= (3*c.ct_byte[0]) << (sft-=2);
  if (!sft) {*P++=buf; buf=0; sft=8;} }
static void sp21_1mn(ColTyp c)
{ buf+= c.ct_byte[0] << (--sft);
  if (!sft) {*P++=buf; buf=0; sft=8;} }
static void sp41_1mw(ColTyp c)
{ buf+= (7*c.ct_byte[0]) << (sft-=4);
  if (!sft) {*P++=buf; buf=0; sft=8;} }
/* sp41_1mn = sp21_1mw */

static void sp22_2mw(ColTyp c0,ColTyp c1)
{ buf+=(4*c0.ct_byte[0] +c1.ct_byte[0]) << (sft-=4);
  if (!sft) {*P++=buf; buf=0; sft=8;} }
static void sp22_2mn(ColTyp c0,ColTyp c1)
{ buf+= c0.ct_byte[0] << (sft-=2);
  if (!sft) {*P++=buf; buf=0; sft=8;} }
static void sp21_2mw(ColTyp c)
{ buf+= (5*c.ct_byte[0]) << (sft-=4);
  if (!sft) {*P++=buf; buf=0; sft=8;} }
static void sp21_2mn(ColTyp c)
{ buf+= c.ct_byte[0] << (sft-=2);
  if (!sft) {*P++=buf; buf=0; sft=8;} }
static void sp41_2w(ColTyp c)
{ *P++=85*c.ct_byte[0]; }
/* sp41_2mn = sp21_2mw */

static void sp22_4mw(ColTyp c0,ColTyp c1)
{ *P++=16*c0.ct_byte[0] + c1.ct_byte[0]; }
static void sp22_4mn(ColTyp c0,ColTyp c1)
{ buf+= c0.ct_byte[0] << (sft-=4);
  if (!sft) {*P++=buf; buf=0; sft=8;} }
static void sp21_4w(ColTyp c)
{ *P++= 17*c.ct_byte[0]; }
static void sp21_4mn(ColTyp c)
{ buf+= c.ct_byte[0] << (sft-=4);
  if (!sft) {*P++=buf; buf=0; sft=8;} }
static void sp41_4w(ColTyp c)
{ *P++= 17*c.ct_byte[0]; *P++= 17*c.ct_byte[0]; }
/* sp41_4mn = sp21_4w */

static void sp22_1lw(ColTyp c0,ColTyp c1)
{ buf+=(c0.ct_byte[0] +2* c1.ct_byte[0]) << sft;
  if ((sft+=2)==8) {*P++=buf; buf=0; sft=0;} }
static void sp22_1ln(ColTyp c0,ColTyp c1)
{ buf+= c0.ct_byte[0] << sft;
  if ((++sft)==8) {*P++=buf; buf=0; sft=0;} }
static void sp21_1lw(ColTyp c)
{ buf+= (3*c.ct_byte[0]) << sft;
  if ((sft+=2)==8) {*P++=buf; buf=0; sft=0;} }
static void sp21_1ln(ColTyp c)
{ buf+= c.ct_byte[0] << sft;
  if ((++sft)==8) {*P++=buf; buf=0; sft=0;} }
static void sp41_1lw(ColTyp c)
{ buf+= (7*c.ct_byte[0]) << sft;
  if ((sft+=4)==8) {*P++=buf; buf=0; sft=0;} }
/* sp41_1ln = sp21_1lw */

static void sp22_2lw(ColTyp c0,ColTyp c1)
{ buf+=(c0.ct_byte[0] +4*c1.ct_byte[0]) << sft;
  if ((sft+=4)==8) {*P++=buf; buf=0; sft=0;} }
static void sp22_2ln(ColTyp c0,ColTyp c1)
{ buf+= c0.ct_byte[0] << sft;
  if ((sft+=2)==8) {*P++=buf; buf=0; sft=0;} }
static void sp21_2lw(ColTyp c)
{ buf+= (5*c.ct_byte[0]) << sft;
  if ((sft+=4)==8) {*P++=buf; buf=0; sft=0;} }
static void sp21_2ln(ColTyp c)
{ buf+= c.ct_byte[0] << sft;
  if ((sft+=2)==8) {*P++=buf; buf=0; sft=0;} }
/* sp41_2w */
/* sp41_2ln = sp21_2lw */

static void sp22_4lw(ColTyp c0,ColTyp c1)
{ *P++=c0.ct_byte[0] + 16*c1.ct_byte[0]; }
static void sp22_4ln(ColTyp c0,ColTyp c1)
{ buf+= c0.ct_byte[0] << sft;
  if ((sft+=4)==8) {*P++=buf; buf=0; sft=0;} }
/* sp21_4w */
static void sp21_4ln(ColTyp c)
{ buf+= c.ct_byte[0] << sft;
  if ((sft+=4)==8) {*P++=buf; buf=0; sft=0;} }
/* sp41_4w */
/* sp41_4mn = sp21_4w */

static void sp22_8w(ColTyp c0, ColTyp c1)
{ *P++=c0.ct_byte[0]; *P++=c1.ct_byte[0]; }
static void sp22_8n(ColTyp c0, ColTyp c1)
{ *P++=c0.ct_byte[0]; }
static void sp21_8w(ColTyp c)
{ *P++=c.ct_byte[0]; *P++=c.ct_byte[0]; }
static void sp21_8n(ColTyp c)
{ *P++=c.ct_byte[0]; }
static void sp41_8w(ColTyp c)
{ *(P+3) = *(P+2) = *(P+1) = *P = c.ct_byte[0]; P+=4; }
/* sp41_8n = sp21_8w */

static void sp22_16w(ColTyp c0, ColTyp c1)
{ *P++=c0.ct_byte[0]; *P++=c0.ct_byte[1];
  *P++=c1.ct_byte[0]; *P++=c1.ct_byte[1]; }
static void sp22_16n(ColTyp c0, ColTyp c1)
{ *P++=c0.ct_byte[0]; *P++=c0.ct_byte[1]; }
static void sp21_16w(ColTyp c)
{ *(P+2) = *P = c.ct_byte[0]; P++;
  *(P+2) = *P = c.ct_byte[1]; P+=3; }
static void sp21_16n(ColTyp c)
{ *P++=c.ct_byte[0]; *P++=c.ct_byte[1]; }
static void sp41_16w(ColTyp c)
{ *(P+6) = *(P+4) = *(P+2) = *P = c.ct_byte[0]; P++;
  *(P+6) = *(P+4) = *(P+2) = *P = c.ct_byte[1]; P+=7; }
/* sp41_16n = sp21_16w */

static void sp22_24w(ColTyp c0, ColTyp c1)
{ *P++=c0.ct_byte[0]; *P++=c0.ct_byte[1]; *P++=c0.ct_byte[2];
  *P++=c1.ct_byte[0]; *P++=c1.ct_byte[1]; *P++=c1.ct_byte[2]; }
static void sp22_24n(ColTyp c0, ColTyp c1)
{ *P++=c0.ct_byte[0]; *P++=c0.ct_byte[1]; *P++=c0.ct_byte[2]; }
static void sp21_24w(ColTyp c)
{ *(P+3) = *P = c.ct_byte[0]; P++;
  *(P+3) = *P = c.ct_byte[1]; P++;
  *(P+3) = *P = c.ct_byte[2]; P+=4; }
static void sp21_24n(ColTyp c)
{ *P++=c.ct_byte[0]; *P++=c.ct_byte[1]; *P++=c.ct_byte[2]; }
static void sp41_24w(ColTyp c)
{ *(P+9) = *(P+6) = *(P+3) = *P = c.ct_byte[0]; P++;
  *(P+9) = *(P+6) = *(P+3) = *P = c.ct_byte[1]; P++;
  *(P+9) = *(P+6) = *(P+3) = *P = c.ct_byte[2]; P+=10; }
/* sp41_24n = sp21_24w */

static void sp22_32w(ColTyp c0, ColTyp c1)
{ *(XID*)P=c0.ct_xid; P+=4; *(XID*)P=c1.ct_xid; P+=4; }
static void sp22_32n(ColTyp c0, ColTyp c1)
{ *(XID*)P=c0.ct_xid; P+=4;}
static void sp21_32w(ColTyp c)
{ *(XID*)P=c.ct_xid; P+=4; *(XID*)P=c.ct_xid; P+=4; }
static void sp21_32n(ColTyp c)
{ *(XID*)P=c.ct_xid; P+=4;}
static void sp41_32w(ColTyp c)
{ *(XID*)P=c.ct_xid; P+=4; *(XID*)P=c.ct_xid; P+=4;
  *(XID*)P=c.ct_xid; P+=4; *(XID*)P=c.ct_xid; P+=4; }
/* sp41_32n = sp21_32w */

/** Pixel setting routines for any number of bits per pixel **/
bp_struct PixSetters[10]= 
{
/* for msb first: */
  { 1, sp22_1mw, sp22_1mn, sp21_1mw, sp21_1mn, sp41_1mw, sp21_1mw},
  { 2, sp22_2mw, sp22_2mn, sp21_2mw, sp21_2mn, sp41_2w, sp21_2mw},
  { 4, sp22_4mw, sp22_4mn, sp21_4w,  sp21_4mn, sp41_4w,  sp21_4w},
/* for lsb first: */ 
  { 0x81, sp22_1lw, sp22_1ln, sp21_1lw, sp21_1ln, sp41_1lw, sp21_1lw},
  { 0x82, sp22_2lw, sp22_2ln, sp21_2lw, sp21_2ln, sp41_2w, sp21_2lw},
  { 0x84, sp22_4lw, sp22_4ln, sp21_4w,  sp21_4ln, sp41_4w,  sp21_4w},
/* Whole bytes, same routines for MSB-first and LSB-first */
  { 8, sp22_8w,  sp22_8n,  sp21_8w,  sp21_8n,  sp41_8w,  sp21_8w},
  {16, sp22_16w, sp22_16n, sp21_16w, sp21_16n, sp41_16w, sp21_16w},
  {24, sp22_24w, sp22_24n, sp21_24w, sp21_24n, sp41_24w, sp21_24w},
  {32, sp22_32w, sp22_32n, sp21_32w, sp21_32n, sp41_32w, sp21_32w},
};

/* duplicates scanlines, used if interlacing is off  */
static void NoIntLac(register byte Y)
{
  register byte *P;
  register word linlen;
  
  linlen=Width*bitpix/8;
  P=XBuf+(long)scale*Y*linlen;
  memcpy(P+linlen,P,linlen);
}

void choosefuncs(int lsbfirst, int bitpix)
{
  int i;

  if (lsbfirst && (bitpix<8)) bitpix|=0x80;
  for (i=0 ; (PixSetters[i].bp_bitpix!=bitpix)&&(i<10) ; i++)
    ;
  funcs=&(PixSetters[i]);
}

void setwidth(int wide)
{
  SeqPix22=wide ? funcs->SeqPix22Wide : funcs->SeqPix22Narrow;
  SeqPix21=wide ? funcs->SeqPix21Wide : funcs->SeqPix21Narrow;
  SeqPix41=wide ? funcs->SeqPix41Wide : funcs->SeqPix41Narrow;
}


#define NOINTLACM1(Y)	NoIntLac((M5HEIGHT-M1HEIGHT)/2+Y)
#define NOINTLACM5(Y)	NoIntLac(Y)
#define SETSCRVARM1(Y)	SetScrVar((M5HEIGHT-M1HEIGHT)/2+Y,(M5WIDTH-M1WIDTH)/2)
#define SETSCRVARM5(Y)	SetScrVar(Y,0)

int IntLac = 0;

/** RefreshScr ***********************************************/
/** Draw window functions for each basic/screen mode        **/
/*************************************************************/

/** RefreshScr10: N60-BASIC select function ******************/
void RefreshScr10(void)
{
  if ((*VRAM&0x80) == 0x00)
    RefreshScr11();
  else
    switch (*(VRAM)&0x1C) {
    case 0x00: case 0x10: /*  64x 64 color / 128x 64 */
      RefreshScr13a(); break;
    case 0x08: /* 128x 64 color */
      RefreshScr13b(); break;
    case 0x18: /* 128x 96 */
      RefreshScr13c(); break;
    case 0x04: /* 128x 96 color */
      RefreshScr13d(); break;
    case 0x14: /* 128x192 */
      RefreshScr13e(); break;
    default: /* 128x192 color / 256x192 */
      RefreshScr13(); break;
    }
}

/** RefreshScr11: N60-BASIC screen 1,2 ***********************/
void RefreshScr11(void)
{
  register byte X,Y,K;
  register ColTyp FC,BC;
  register byte *S,*T1,*T2;
  byte *G;
  
  G = CGROM;		/* CGROM */ 
  T1 = VRAM;		/* attribute data */
  T2 = VRAM+0x0200;	/* ascii/semi-graphic data */
  for(Y=0; Y<M1HEIGHT; Y++) {
    SETSCRVARM1(Y);	/* Drawing area */
    for(X=0; X<32; X++, T1++, T2++) {
      /* get CGROM address and color */
      if (*T1&0x40) {	/* if semi-graphic */
	if (*T1&0x20) {		/* semi-graphic 6 */
	  S = G+((*T2&0x3f)<<4)+0x1000;
	  FC = BPal12[(*T1&0x02)<<1|(*T2)>>6]; BC = BPal[8];
	} else {		/* semi-graphic 4 */
	  S = G+((*T2&0x0f)<<4)+0x2000;
	  FC = BPal12[(*T2&0x70)>>4]; BC = BPal[8];
	}
      } else {		/* if normal character */
	S = G+((*T2)<<4); 
	FC = BPal11[(*T1&0x03)]; BC = BPal11[(*T1&0x03)^0x01];
      }
      K=*(S+Y%12);
      SeqPix21(K&0x80? FC:BC); SeqPix21(K&0x40? FC:BC);
      SeqPix21(K&0x20? FC:BC); SeqPix21(K&0x10? FC:BC);
      SeqPix21(K&0x08? FC:BC); SeqPix21(K&0x04? FC:BC);
      SeqPix21(K&0x02? FC:BC); SeqPix21(K&0x01? FC:BC);
    }
    if ((scale==2) && !IntLac) NOINTLACM1(Y);
    if (Y%12!=11) { T1-=32; T2-=32; }
  }
  if(EndOfFrame) PutImage();
}

/** RefreshScr13: N60-BASIC screen 3,4 ***********************/
void RefreshScr13(void)
{
  register byte X,Y;
  register byte *T1,*T2;
  byte attr;

  T1 = VRAM;		/* attribute data */
  T2 = VRAM+0x0200;	/* graphic data */
  for (Y=0; Y<M1HEIGHT; Y++) {
    SETSCRVARM1(Y);	/* Drawing area */
    for (X=0; X<32; X++,T1++,T2++) {
      if (*T1&0x10) { /* 256x192 (SCREEN 4) */
	if (scr4col) {
	  attr = (*T1&0x02)<<1;
	  SeqPix41(BPal15[attr|(*T2&0xC0)>>6]);
	  SeqPix41(BPal15[attr|(*T2&0x30)>>4]);
	  SeqPix41(BPal15[attr|(*T2&0x0C)>>2]);
	  SeqPix41(BPal15[attr|(*T2&0x03)   ]);
	} else {
	  attr = *T1&0x02;
	  SeqPix21(BPal14[attr|(*T2&0x80)>>7]);
	  SeqPix21(BPal14[attr|(*T2&0x40)>>6]);
	  SeqPix21(BPal14[attr|(*T2&0x20)>>5]);
	  SeqPix21(BPal14[attr|(*T2&0x10)>>4]);
	  SeqPix21(BPal14[attr|(*T2&0x08)>>3]);
	  SeqPix21(BPal14[attr|(*T2&0x04)>>2]);
	  SeqPix21(BPal14[attr|(*T2&0x02)>>1]);
	  SeqPix21(BPal14[attr|(*T2&0x01)   ]);
	}
      } else { /* 128x192 color (SCREEN 3) */
	attr = (*T1&0x02)<<1;
	SeqPix41(BPal13[attr|(*T2&0xC0)>>6]);
	SeqPix41(BPal13[attr|(*T2&0x30)>>4]);
	SeqPix41(BPal13[attr|(*T2&0x0C)>>2]);
	SeqPix41(BPal13[attr|(*T2&0x03)   ]);
      }
    }
    if (T1 == VRAM+0x200) T1=VRAM;
    if ((scale==2) && !IntLac) NOINTLACM1(Y);
  }
  if(EndOfFrame) PutImage();
}

/** RefreshScr13a: N60-BASIC screen 3,4 **********************/
void RefreshScr13a(void) /*  64x 64 color / 128x 64 */
{
  register byte X,Y;
  register byte *T1,*T2;
  byte attr;
  ColTyp L;

  T1 = VRAM;		/* attribute data */
  T2 = VRAM+0x0200;	/* graphic data */
  for (Y=0; Y<M1HEIGHT; Y++) {
    SETSCRVARM1(Y);	/* Drawing area */
    for (X=0; X<16; X++,T1++,T2++) {
      if (*T1&0x10) { /* 128x 64 */
	if (scr4col) {
	  attr = (*T1&0x02)<<1;
	  SeqPix41(L=BPal15[attr|(*T2&0xC0)>>6]);
	  SeqPix41(L);
	  SeqPix41(L=BPal15[attr|(*T2&0x30)>>4]);
	  SeqPix41(L);
	  SeqPix41(L=BPal15[attr|(*T2&0x0C)>>2]);
	  SeqPix41(L);
	  SeqPix41(L=BPal15[attr|(*T2&0x03)   ]);
	  SeqPix41(L);
	} else { /*  64x 64 color */
	  attr = *T1&0x02;
	  SeqPix41(BPal14[attr|(*T2&0x80)>>7]);
	  SeqPix41(BPal14[attr|(*T2&0x40)>>6]);
	  SeqPix41(BPal14[attr|(*T2&0x20)>>5]);
	  SeqPix41(BPal14[attr|(*T2&0x10)>>4]);
	  SeqPix41(BPal14[attr|(*T2&0x08)>>3]);
	  SeqPix41(BPal14[attr|(*T2&0x04)>>2]);
	  SeqPix41(BPal14[attr|(*T2&0x02)>>1]);
	  SeqPix41(BPal14[attr|(*T2&0x01)   ]);
	}
      } else { /*  64x 64 color */
	attr = (*T1&0x02)<<1;
	SeqPix41(L=BPal13[attr|(*T2&0xC0)>>6]);
	SeqPix41(L);
	SeqPix41(L=BPal13[attr|(*T2&0x30)>>4]);
	SeqPix41(L);
	SeqPix41(L=BPal13[attr|(*T2&0x0C)>>2]);
	SeqPix41(L);
	SeqPix41(L=BPal13[attr|(*T2&0x03)   ]);
	SeqPix41(L);
      }
    }
    if (Y%3 != 2) { T1-=16; T2-=16; }
    else if (T1 == VRAM+0x200) T1=VRAM;
    if ((scale==2) && !IntLac) NOINTLACM1(Y);
  }
  if(EndOfFrame) PutImage();
}

/** RefreshScr13b: N60-BASIC screen 3,4 **********************/
void RefreshScr13b(void) /* 128x 64 color */
{
  register byte X,Y;
  register byte *T1,*T2;
  byte attr;

  T1 = VRAM;		/* attribute data */
  T2 = VRAM+0x0200;	/* graphic data */
  for (Y=0; Y<M1HEIGHT; Y++) {
    SETSCRVARM1(Y);	/* Drawing area */
    for (X=0; X<32; X++,T1++,T2++) {
      attr = (*T1&0x02)<<1;
      SeqPix41(BPal13[attr|(*T2&0xC0)>>6]);
      SeqPix41(BPal13[attr|(*T2&0x30)>>4]);
      SeqPix41(BPal13[attr|(*T2&0x0C)>>2]);
      SeqPix41(BPal13[attr|(*T2&0x03)   ]);
    }
    if (Y%3 != 2) { T1-=32; T2-=32; }
    else if (T1 == VRAM+0x200) T1=VRAM;
    if ((scale==2) && !IntLac) NOINTLACM1(Y);
  }
  if(EndOfFrame) PutImage();
}

/** RefreshScr13c: N60-BASIC screen 3,4 **********************/
void RefreshScr13c(void) /* 128x 96 */
{
  register byte X,Y;
  register byte *T1,*T2;
  byte attr;
  ColTyp L;

  T1 = VRAM;		/* attribute data */
  T2 = VRAM+0x0200;	/* graphic data */
  for (Y=0; Y<M1HEIGHT; Y++) {
    SETSCRVARM1(Y);	/* Drawing area */
    for (X=0; X<16; X++,T1++,T2++) {
      if (scr4col) {
	attr = (*T1&0x02)<<1;
	SeqPix41(L=BPal15[attr|(*T2&0xC0)>>6]);
	SeqPix41(L);
	SeqPix41(L=BPal15[attr|(*T2&0x30)>>4]);
	SeqPix41(L);
	SeqPix41(L=BPal15[attr|(*T2&0x0C)>>2]);
	SeqPix41(L);
	SeqPix41(L=BPal15[attr|(*T2&0x03)   ]);
	SeqPix41(L);
      } else {
	attr = *T1&0x02;
	SeqPix41(BPal14[attr|(*T2&0x80)>>7]);
	SeqPix41(BPal14[attr|(*T2&0x40)>>6]);
	SeqPix41(BPal14[attr|(*T2&0x20)>>5]);
	SeqPix41(BPal14[attr|(*T2&0x10)>>4]);
	SeqPix41(BPal14[attr|(*T2&0x08)>>3]);
	SeqPix41(BPal14[attr|(*T2&0x04)>>2]);
	SeqPix41(BPal14[attr|(*T2&0x02)>>1]);
	SeqPix41(BPal14[attr|(*T2&0x01)   ]);
      }
    }
    if (!(Y&1)) { T1-=16; T2-=16; }
    else if (T1 == VRAM+0x200) T1=VRAM;
    if ((scale==2) && !IntLac) NOINTLACM1(Y);
  }
  if(EndOfFrame) PutImage();
}

/** RefreshScr13d: N60-BASIC screen 3,4 **********************/
void RefreshScr13d(void) /* 128x 96 color */
{
  register byte X,Y;
  register byte *T1,*T2;
  byte attr;

  T1 = VRAM;		/* attribute data */
  T2 = VRAM+0x0200;	/* graphic data */
  for (Y=0; Y<M1HEIGHT; Y++) {
    SETSCRVARM1(Y);	/* Drawing area */
    for (X=0; X<32; X++,T1++,T2++) {
      attr = (*T1&0x02)<<1;
      SeqPix41(BPal13[attr|(*T2&0xC0)>>6]);
      SeqPix41(BPal13[attr|(*T2&0x30)>>4]);
      SeqPix41(BPal13[attr|(*T2&0x0C)>>2]);
      SeqPix41(BPal13[attr|(*T2&0x03)   ]);
    }
    if (!(Y&1)) { T1-=32; T2-=32; }
    else if (T1 == VRAM+0x200) T1=VRAM;
    if ((scale==2) && !IntLac) NOINTLACM1(Y);
  }
  if(EndOfFrame) PutImage();
}

/** RefreshScr13e: N60-BASIC screen 3,4 **********************/
void RefreshScr13e(void) /* 128x192 */
{
  register byte X,Y;
  register byte *T1,*T2;
  byte attr;
  ColTyp L;

  T1 = VRAM;		/* attribute data */
  T2 = VRAM+0x0200;	/* graphic data */
  for (Y=0; Y<M1HEIGHT; Y++) {
    SETSCRVARM1(Y);	/* Drawing area */
    for (X=0; X<16; X++,T1++,T2++) {
      if (scr4col) {
	attr = (*T1&0x02)<<1;
	SeqPix41(L=BPal15[attr|(*T2&0xC0)>>6]);
	SeqPix41(L);
	SeqPix41(L=BPal15[attr|(*T2&0x30)>>4]);
	SeqPix41(L);
	SeqPix41(L=BPal15[attr|(*T2&0x0C)>>2]);
	SeqPix41(L);
	SeqPix41(L=BPal15[attr|(*T2&0x03)   ]);
	SeqPix41(L);
      } else {
	attr = *T1&0x02;
	SeqPix41(BPal14[attr|(*T2&0x80)>>7]);
	SeqPix41(BPal14[attr|(*T2&0x40)>>6]);
	SeqPix41(BPal14[attr|(*T2&0x20)>>5]);
	SeqPix41(BPal14[attr|(*T2&0x10)>>4]);
	SeqPix41(BPal14[attr|(*T2&0x08)>>3]);
	SeqPix41(BPal14[attr|(*T2&0x04)>>2]);
	SeqPix41(BPal14[attr|(*T2&0x02)>>1]);
	SeqPix41(BPal14[attr|(*T2&0x01)   ]);
      }
    }
    if (T1 == VRAM+0x200) T1=VRAM;
    if ((scale==2) && !IntLac) NOINTLACM1(Y);
  }
  if(EndOfFrame) PutImage();
}

/** RefreshScr51: N60m/66-BASIC screen 1,2 *******************/
void RefreshScr51(void)
{
  register byte X,Y,K;
  register ColTyp FC,BC;
  register byte *S,*T1,*T2;
  byte *G;

  G = CGROM;		/* CGROM */ 
  T1 = VRAM;		/* attribute data */
  T2 = VRAM+0x0400;	/* ascii/semi-graphic data */
  for(Y=0; Y<M5HEIGHT; Y++) {
    SETSCRVARM5(Y);	/* Drawing area */
    for(X=0; X<40; X++, T1++, T2++) {
      /* get CGROM address and color */
      S = G+(*T2<<4)+(*T1&0x80?0x1000:0);
      FC = BPal[(*T1)&0x0F]; BC = BPal[(((*T1)&0x70)>>4)|CSS2];
      K=*(S+Y%10);
      SeqPix21(K&0x80? FC:BC); SeqPix21(K&0x40? FC:BC);
      SeqPix21(K&0x20? FC:BC); SeqPix21(K&0x10? FC:BC);
      SeqPix21(K&0x08? FC:BC); SeqPix21(K&0x04? FC:BC);
      SeqPix21(K&0x02? FC:BC); SeqPix21(K&0x01? FC:BC);
    }
    if ((scale==2) && !IntLac) NOINTLACM5(Y);
    if (Y%10!=9) { T1-=40; T2-=40; }
  }
  if(EndOfFrame) PutImage();
}

/** RefreshScr53: N60m/66-BASIC screen 3 *********************/
void RefreshScr53(void)
{
  register byte X,Y;
  register byte *T1,*T2;
  
  T1 = VRAM;		/* attribute data */
  T2 = VRAM+0x2000;	/* graphic data */
  for(Y=0; Y<M5HEIGHT; Y++) {
    SETSCRVARM5(Y);	/* Drawing area */
    for(X=0; X<40; X++) {
      SeqPix41(BPal53[CSS3|((*T1)&0xC0)>>6|((*T2)&0xC0)>>4]);
      SeqPix41(BPal53[CSS3|((*T1)&0x30)>>4|((*T2)&0x30)>>2]);
      SeqPix41(BPal53[CSS3|((*T1)&0x0C)>>2|((*T2)&0x0C)   ]);
      SeqPix41(BPal53[CSS3|((*T1)&0x03)   |((*T2)&0x03)<<2]);
      T1++; T2++;
    }
    if ((scale==2) && !IntLac) NOINTLACM5(Y);
  }
  if(EndOfFrame) PutImage();
}

/** RefreshScr54: N60m/66-BASIC screen 4 *********************/
void RefreshScr54(void)
{
  register byte X,Y;
  register byte *T1,*T2;
  byte cssor;

  T1 = VRAM;		/* attribute data */
  T2 = VRAM+0x2000;	/* graphic data */
  /* CSS OR */
  cssor = CSS3|CSS2|CSS1;
  for(Y=0; Y<M5HEIGHT; Y++) {
    SETSCRVARM5(Y);	/* Drawing area */
    for(X=0; X<40; X++) {
      SeqPix21(BPal53[cssor|((*T1)&0x80)>>7|((*T2)&0x80)>>6]);
      SeqPix21(BPal53[cssor|((*T1)&0x40)>>6|((*T2)&0x40)>>5]);
      SeqPix21(BPal53[cssor|((*T1)&0x20)>>5|((*T2)&0x20)>>4]);
      SeqPix21(BPal53[cssor|((*T1)&0x10)>>4|((*T2)&0x10)>>3]);
      SeqPix21(BPal53[cssor|((*T1)&0x08)>>3|((*T2)&0x08)>>2]);
      SeqPix21(BPal53[cssor|((*T1)&0x04)>>2|((*T2)&0x04)>>1]);
      SeqPix21(BPal53[cssor|((*T1)&0x02)>>1|((*T2)&0x02)   ]);
      SeqPix21(BPal53[cssor|((*T1)&0x01)   |((*T2)&0x01)<<1]);
      T1++;T2++;
    }
    if ((scale==2) && !IntLac) NOINTLACM5(Y);
  }
  if(EndOfFrame) PutImage();
}
