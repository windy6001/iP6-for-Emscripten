/** iP6: PC-6000/6600 series emualtor ************************/
/**                                                         **/
/**                           P6.c                          **/
/**                                                         **/
/** by ISHIOKA Hiroshi 1998-2000                            **/
/** This code is based on fMSX written by Marat Fayzullin   **/
/*************************************************************/
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "P6.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

#define PRINTOK     if(Verbose) printf(MsgOK)
#define PRINTFAILED if(Verbose) printf(MsgFAILED)

#define CAS_NONE	0
#define CAS_SAVEBYTE	1
#define CAS_LOADING	2
#define CAS_LOADBYTE	3

char *MsgOK      = "OK\n";
char *MsgFAILED  = "FAILED\n";

int P6Version;
int  PatchLevel = 1;
int CGSW93 = FALSE;
int portF0 = 0x11;
int portF3 = 0;
int portF6 = 0;
int portF7 = 0x06;		/* 初期値は0x06とする (PC-6001対応) */

byte Verbose = 1;
byte *BASICROM = NULL;
byte *VOICEROM = NULL;
byte *KANJIROM = NULL;
byte *CurKANJIROM = NULL;
/*
byte *ROM2 = NULL;
*/
byte *SYSROM2 = NULL;
/* 99.06.02.*/

byte *CGROM = NULL;
byte *CGROM1 = NULL;
byte *CGROM5 = NULL;
byte *EXTROM = NULL;
byte *EXTROM1 = NULL;
byte *EXTROM2 = NULL;
char Ext1Name[FILENAME_MAX] = "";    /* Extension ROM 1 file */
char Ext2Name[FILENAME_MAX] = "";    /* Extension ROM 2 file */
byte *RdMem[8];
byte *WrMem[8];
byte *VRAM;
static unsigned int VRAMHead[2][4] = {
  { 0xc000, 0xe000, 0x8000, 0xa000 },
  { 0x8000, 0xc000, 0x0000, 0x4000 },
};
byte *EmptyRAM;
byte *RAM;
byte EnWrite[4];
byte CRTMode1,CRTMode2,CRTMode3;

byte PSGReg=0;
byte PSG[16];
byte JoyState[2];
byte CSS1,CSS2,CSS3;

byte UPeriod     = 2;               /* Interrupts/scr. update */
byte EndOfFrame=1;                  /* 1 when end of frame    */

byte p6key = 0;
byte stick0 = 0;
byte keyGFlag = 0;
byte kanaMode = 0;
byte katakana = 0;
byte kbFlagGraph = 0;
byte kbFlagCtrl = 0;
byte TimerSW = 0;
byte TimerSW_F3 = 1;		/* 初期値は1とする（PC-6001対応） */

int IntSW_F3 = 1;
int Code16Count = 0;

/*char *PrnName    = NULL;*/            /* Printer redirect. file */
char PrnName[FILENAME_MAX] = "";    /* Printer redirect. file */
FILE *PrnStream  = NULL;

byte CasMode = CAS_NONE;
/*char *CasName    = "DEFAULT.CAS";*/   /* Tape image file        */
char CasName[FILENAME_MAX] = "";    /* Tape image file        */
FILE *CasStream  = NULL;

char DskName[FILENAME_MAX] = "";    /* Disk image file        */
FILE *DskStream  = NULL;

void LockSurface(void);
void UnlockSurface(void);

/******** Screen Mode Handlers **********************************/
extern void (*SCR[2][4])() ;

/* Load a ROM file into given pointer */
byte *LoadROM(byte **mem, char *name, int size);

int LoadEXTROM(byte *mem, char *name, int size);

void Printer(byte V);             /* Send a character to a printer   */

/****************************************************************/
/*** This function is called when a read from RAM occurs.     ***/
/****************************************************************/
byte M_RDMEM(register word A)
{
  return(RdMem[A>>13][A&0x1FFF]);
}

/****************************************************************/
/*** This function is called when a write to RAM occurs. It   ***/
/*** checks for write protection and slot selectors.          ***/
/****************************************************************/
void M_WRMEM(register word A,register byte V)
{
  if(EnWrite[A>>14]) WrMem[A>>13][A&0x1FFF]=V;
#ifdef DEBUG
  else printf("M_WRMEM:%4X\n",A);
#endif
}

/****************************************************************/
/*** Write number into given IO port.                         ***/
/****************************************************************/
void DoOut(register byte Port,register byte Value)
{
  if (Verbose&0x02) printf("--- DoOut : %02X, %02X ---\n", Port, Value);
  switch(Port) {
    case 0x80: return;
    case 0x81: return;                   /* 8251 status          */
    case 0x90:                           /* subCPU               */
      if (CasMode==CAS_SAVEBYTE) /* CMT SAVE */
	{ fputc(Value,CasStream); CasMode=CAS_NONE; return; }
      if ((Value==0x1a)&&(CasMode==CAS_LOADING)) /* CMT STOP */
	{ CasMode=CAS_NONE; return; }
      if ((Value==0x19)&&(CasStream))
	/* CMT LOAD OPEN(0x1E,0x19(1200baud)/0x1D,0x19(600baud)) */
	{ CasMode=CAS_LOADING; return; }
      if ((Value==0x38)&&(CasStream)) /* CMT SAVE DATA */
	{ CasMode=CAS_SAVEBYTE; return; }
      if (Value==0x04) { kanaMode = kanaMode ? 0 : 1; return; }
      if (Value==0x05) { katakana = katakana ? 0 : 1; return; }
      if (Value==0x06) /* strig,stick */
	{ StrigIntFlag = INTFLAG_REQ; return; }
      return;
    case 0x91: Printer(~Value); return;  /* printer data         */
    case 0x92: return;                   /* printer,CRT,CG,sub   */
    case 0x93: 
      switch(Value) {
	/*
	case 0x02: EndOfFrame=0; break;
	case 0x03: EndOfFrame=1; break;
	*/
	case 0x04: CGSW93 = TRUE; RdMem[3]=CGROM; break;
	case 0x05: CGSW93 = FALSE; DoOut(0xF0,portF0); break;
      }
      return;
    case 0xA0: PSGReg=Value&0x0F;return; /* 8910 reg addr        */
    case 0xA1: PSG[PSGReg]=Value;        /* 8910 data            */
      PSGOut(PSGReg,Value);
      return;
    case 0xA3: return;                   /* 8910 inactive ?      */
    case 0xB0:
      VRAM=RAM+VRAMHead[CRTMode1][(Value&0x06)>>1];
      TimerSW=(Value&0x01)?0:1;
      return;
    case 0xC0:                           /* CSS                  */
      CSS3=(Value&0x04)<<2;CSS2=(Value&0x02)<<2;CSS1=(Value&0x01)<<2;
      return;
    case 0xC1:                           /* CRT controller mode  */
      if((CRTMode1)&&(Value&0x02)) ClearScr();
      CRTMode1=(Value&0x02) ? 0 : 1;
      CRTMode2=(Value&0x04) ? 0 : 1;
      CRTMode3=(Value&0x08) ? 0 : 1;
      CGROM = ((CRTMode1 == 0) ? CGROM1 : CGROM5);
      return;
    case 0xC2:                           /* ROM swtich           */
      if ((Value&0x02)==0x00) CurKANJIROM=KANJIROM;
      else CurKANJIROM=KANJIROM+0x4000;
      if ((Value&0x01)==0x00) {
	// --- 2000.10.13.
        if(P6Version==3) {
          if(RdMem[0]!=BASICROM) RdMem[0]=VOICEROM;
          if(RdMem[1]!=BASICROM+0x2000) RdMem[1]=VOICEROM+0x2000;
        };
	// --- 2000.10.13.
	if(RdMem[2]!=BASICROM+0x4000) RdMem[2]=VOICEROM;
	if(RdMem[3]!=BASICROM+0x6000) RdMem[3]=VOICEROM+0x2000;
      }      

      /*
      else {RdMem[0]=CurKANJIROM;RdMem[1]=CurKANJIROM+0x2000;};
      */
      else {
	DoOut(0xF0,portF0); 	
      };
      /* 99.06.02. */

      return;
    case 0xC3: return;                   /* C2H in/out switch    */
    case 0xD1: return;                   /* disk command,data    */
    case 0xD3: return;                   /* disk signal          */
    case 0xF0:                           /* read block set       */
      portF0 = Value;
      switch(Value&0x0f) {
	case 0x00: RdMem[0]=RdMem[1]=EmptyRAM; break;
	case 0x01: RdMem[0]=BASICROM;RdMem[1]=BASICROM+0x2000; break;
	case 0x02: RdMem[0]=CurKANJIROM;RdMem[1]=CurKANJIROM+0x2000; break;
	case 0x03: RdMem[0]=RdMem[1]=EXTROM2; break;
	case 0x04: RdMem[0]=RdMem[1]=EXTROM1; break;
	case 0x05: RdMem[0]=CurKANJIROM;RdMem[1]=BASICROM+0x2000; break;

	  /*
	case 0x06: RdMem[0]=BASICROM;RdMem[1]=CurKANJIROM+0x2000; break;
	*/
	case 0x06:
	  RdMem[0]=BASICROM;
	  RdMem[1]=(SYSROM2==EmptyRAM ? CurKANJIROM+0x2000 : SYSROM2);
	  break;
	  /* 99.06.02. */

	case 0x07: RdMem[0]=EXTROM1;RdMem[1]=EXTROM2; break;
	case 0x08: RdMem[0]=EXTROM2;RdMem[1]=EXTROM1; break;
	case 0x09: RdMem[0]=EXTROM2;RdMem[1]=BASICROM+0x2000; break;
	case 0x0a: RdMem[0]=BASICROM;RdMem[1]=EXTROM2; break;
	case 0x0b: RdMem[0]=EXTROM1;RdMem[1]=CurKANJIROM+0x2000; break;
	case 0x0c: RdMem[0]=CurKANJIROM;RdMem[1]=EXTROM1; break;
	case 0x0d: RdMem[0]=RAM;RdMem[1]=RAM+0x2000; break;
	case 0x0e: RdMem[0]=RdMem[1]=EmptyRAM; break;
	case 0x0f: RdMem[0]=RdMem[1]=EmptyRAM; break;
      };
      switch(Value&0xf0) {
	case 0x00: RdMem[2]=RdMem[3]=EmptyRAM; break;
	case 0x10: RdMem[2]=BASICROM+0x4000;RdMem[3]=BASICROM+0x6000; break;
	case 0x20: RdMem[2]=VOICEROM;RdMem[3]=VOICEROM+0x2000; break;
	case 0x30: RdMem[2]=RdMem[3]=EXTROM2; break;
	case 0x40: RdMem[2]=RdMem[3]=EXTROM1; break;
	case 0x50: RdMem[2]=VOICEROM;RdMem[3]=BASICROM+0x6000; break;
	case 0x60: RdMem[2]=BASICROM+0x4000;RdMem[3]=VOICEROM+0x2000; break;
	case 0x70: RdMem[2]=EXTROM1;RdMem[3]=EXTROM2; break;
	case 0x80: RdMem[2]=EXTROM2;RdMem[3]=EXTROM1; break;
	case 0x90: RdMem[2]=EXTROM2;RdMem[3]=BASICROM+0x6000; break;
	case 0xa0: RdMem[2]=BASICROM+0x4000;RdMem[3]=EXTROM2; break;
	case 0xb0: RdMem[2]=EXTROM1;RdMem[3]=VOICEROM+0x2000; break;
	case 0xc0: RdMem[2]=VOICEROM;RdMem[3]=EXTROM1; break;
	case 0xd0: RdMem[2]=RAM+0x4000;RdMem[3]=RAM+0x6000; break;
	case 0xe0: RdMem[2]=RdMem[3]=EmptyRAM; break;
	case 0xf0: RdMem[2]=RdMem[3]=EmptyRAM; break;
      };
      if (CGSW93) RdMem[3] = CGROM;
      return;
    case 0xF1:                           /* read block set       */
      switch(Value&0x0f) {
	case 0x00: RdMem[4]=RdMem[5]=EmptyRAM; break;
	case 0x01: RdMem[4]=BASICROM;RdMem[5]=BASICROM+0x2000; break;
	case 0x02: RdMem[4]=CurKANJIROM;RdMem[5]=CurKANJIROM+0x2000; break;
	case 0x03: RdMem[4]=RdMem[5]=EXTROM2; break;
	case 0x04: RdMem[4]=RdMem[5]=EXTROM1; break;
	case 0x05: RdMem[4]=CurKANJIROM;RdMem[5]=BASICROM+0x2000; break;
	case 0x06: RdMem[4]=BASICROM;RdMem[5]=CurKANJIROM+0x2000; break;
	case 0x07: RdMem[4]=EXTROM1;RdMem[5]=EXTROM2; break;
	case 0x08: RdMem[4]=EXTROM2;RdMem[5]=EXTROM1; break;
	case 0x09: RdMem[4]=EXTROM2;RdMem[5]=BASICROM+0x2000; break;
	case 0x0a: RdMem[4]=BASICROM;RdMem[5]=EXTROM2; break;
	case 0x0b: RdMem[4]=EXTROM1;RdMem[5]=CurKANJIROM+0x2000; break;
	case 0x0c: RdMem[4]=CurKANJIROM;RdMem[5]=EXTROM1; break;
	case 0x0d: RdMem[4]=RAM+0x8000;RdMem[5]=RAM+0xa000; break;
	case 0x0e: RdMem[4]=RdMem[5]=EmptyRAM; break;
	case 0x0f: RdMem[4]=RdMem[5]=EmptyRAM; break;
      };
      switch(Value&0xf0) {
	case 0x00: RdMem[6]=RdMem[7]=EmptyRAM; break;
	case 0x10: RdMem[6]=BASICROM+0x4000;RdMem[7]=BASICROM+0x6000; break;
	case 0x20: RdMem[6]=CurKANJIROM;RdMem[7]=CurKANJIROM+0x2000; break;
	case 0x30: RdMem[6]=RdMem[7]=EXTROM2; break;
	case 0x40: RdMem[6]=RdMem[7]=EXTROM1; break;
	case 0x50: RdMem[6]=CurKANJIROM;RdMem[7]=BASICROM+0x6000; break;
	case 0x60: RdMem[6]=BASICROM+0x4000;RdMem[7]=CurKANJIROM+0x2000; break;
	case 0x70: RdMem[6]=EXTROM1;RdMem[7]=EXTROM2; break;
	case 0x80: RdMem[6]=EXTROM2;RdMem[7]=EXTROM1; break;
	case 0x90: RdMem[6]=EXTROM2;RdMem[7]=BASICROM+0x6000; break;
	case 0xa0: RdMem[6]=BASICROM+0x4000;RdMem[7]=EXTROM2; break;
	case 0xb0: RdMem[6]=EXTROM1;RdMem[7]=CurKANJIROM+0x2000; break;
	case 0xc0: RdMem[6]=CurKANJIROM;RdMem[7]=EXTROM1; break;
	case 0xd0: RdMem[6]=RAM+0xc000;RdMem[7]=RAM+0xe000; break;
	case 0xe0: RdMem[6]=RdMem[7]=EmptyRAM; break;
	case 0xf0: RdMem[6]=RdMem[7]=EmptyRAM; break;
      };
      return;
    case 0xF2:                           /* write ram block set  */
      if(Value&0x40) {EnWrite[3]=1;WrMem[6]=RAM+0xc000;WrMem[7]=RAM+0xe000;}
      else EnWrite[3]=0;
      if(Value&0x010) {EnWrite[2]=1;WrMem[4]=RAM+0x8000;WrMem[5]=RAM+0xa000;}
      else EnWrite[2]=0;
      if(Value&0x04) {EnWrite[1]=1;WrMem[2]=RAM+0x4000;WrMem[3]=RAM+0x6000;}
      else EnWrite[1]=0;
      if(Value&0x01) {EnWrite[0]=1;WrMem[0]=RAM;WrMem[1]=RAM+0x2000;}
      else EnWrite[0]=0;
      return;
    case 0xF3:                           /* wait,int control     */
      portF3 = Value;
      TimerSW_F3=(Value&0x04)?0:1;
      IntSW_F3=(Value&0x01)?0:1;
      return;
    case 0xF4: return;                   /* int1 addr set        */
    case 0xF5: return;                   /* int2 addr set        */
    case 0xF6:                           /* timer countup value  */
      portF6 = Value;
      SetTimerIntClock(Value);
      return;
    case 0xF7:                           /* tmer int addr set    */
      portF7 = Value;
      return;
    case 0xF8: return;                   /* CG access control    */
  }
}

/****************************************************************/
/*** Read number from given IO port.                          ***/
/****************************************************************/
byte DoIn(register byte Port)
{
  /*static byte Last90Data = 0;*/
  byte Value;
  switch(Port) {
    case 0x80: Value=NORAM;break;
    case 0x81: Value=0x85;break;         /* 8251 ACIA status     */
    case 0x90:                           /* subCPU               */
      if ((CmtIntFlag == INTFLAG_EXEC) && (CasMode==CAS_LOADBYTE)) {
	/* CMT Load 1 Byte */
	CmtIntFlag = INTFLAG_NONE;
	CasMode=CAS_LOADING;
	Value=fgetc(CasStream);
      } else if (StrigIntFlag == INTFLAG_EXEC) {
	/* stick,strig */
	StrigIntFlag = INTFLAG_NONE;
	Value = stick0;
	/*
	if (ExecStringInt) {
		Value=stick0;
		ExecStringInt = 0;
		Code16Count = 0;
	} else {
		Code16Count++;
		if (10 == Code16Count) {
			Value = 0x16;
			Code16Count = 0;
		} else {
			Value = stick0;
		}
	}
	*/
      } else if (KeyIntFlag == INTFLAG_EXEC) {
	/* keyboard */
	KeyIntFlag = INTFLAG_NONE;
	if ((p6key == 0xFE) && (keyGFlag == 1)) kanaMode = kanaMode ? 0 : 1;
	if ((p6key == 0xFC) && (keyGFlag == 1)) katakana = katakana ? 0 : 1;
	Value = p6key;
	/*
	if (0 != p6key) Last90Data = Value;
	p6key = 0;
	*/
      } else {
        /*if (0 == p6key) Value = Last90Data; else Value = p6key;*/
	Value = p6key;
      }
      break;
    case 0x92: Value=NORAM;break;        /* printer,CRT,CG,sub   */
    case 0x93: Value=NORAM;break;        /* 8255 mode, portC     */
    case 0xA2:                           /* 8910 data */
      if(PSGReg!=14) Value=(PSGReg>13? 0xff:PSG[PSGReg]);
      else
  	if(PSG[15]==0xC0) Value=JoyState[0];
	else              Value=JoyState[1];
      break;
    case 0xC0: Value=NORAM;break;        /* pr busy,rs carrier   */
    case 0xC2: Value=NORAM;break;        /* ROM switch           */
    case 0xD0: Value=NORAM;break;        /* disk data            */
    case 0xD2: Value=NORAM;break;        /* disk signal          */
    case 0xE0: Value=0x40;break;         /* uPD7752              */
    case 0xF0: Value=portF0;break;       /* read block set       */
    case 0xF3: Value=portF3;break;
    case 0xF6: Value=portF6;break;
    case 0xF7: Value=portF7;break;
    default:   Value=NORAM;break;        /* If no port, ret FFh  */
  }
  if (Verbose&0x02) printf("--- DoIn  : %02X, %02X ---\n", Port, Value);
  return(Value);
}


/****************************************************************/
/*** Allocate memory, load ROM images, initialize mapper, VDP ***/
/*** CPU and start the emulation. This function returns 0 in  ***/
/*** the case of failure.                                     ***/
/****************************************************************/
int StartP6(void)
{
  word A;
  int *T,J,K;
  byte *P;

    /*
  byte **ROMList[5] = { &BASICROM, &CGROM1, &CGROM5, &KANJIROM, &VOICEROM };
  char *ROMName[5][5] = {
    {"BASICROM.60","CGROM60.60",""           ,""           ,""           },
    {"BASICROM.62","CGROM60.62","CGROM60m.62","KANJIROM.62","VOICEROM.62"},
    {"BASICROM.64","CGROM60.64","CGROM66.64" ,"KANJIROM.64","VOICEROM.64"},
    {"BASICROM.66","CGROM60.66","CGROM66.66" ,"KANJIROM.66","VOICEROM.66"},
    {"BASICROM.68","CGROM60.68","CGROM66.68" ,"KANJIROM.68","VOICEROM.68"},
    */
  byte **ROMList[6] = { &BASICROM, &CGROM1, &CGROM5, &KANJIROM, &VOICEROM,
			&SYSROM2 };
  char *ROMName[5][6] = {
    {"BASICROM.60","CGROM60.60",""           ,""           ,""           ,""},
    {"BASICROM.62","CGROM60.62","CGROM60m.62","KANJIROM.62","VOICEROM.62",""},
    {"BASICROM.64","CGROM60.64","CGROM66.64" ,"KANJIROM.64","VOICEROM.64",
     "SYSROM2.64"},
    {"BASICROM.66","CGROM60.66","CGROM66.66" ,"KANJIROM.66","VOICEROM.66",""},
    {"BASICROM.68","CGROM60.68","CGROM66.68" ,"KANJIROM.68","VOICEROM.68",
     "SYSROM2.68"},
    /* 99.06.02. */

  };

    /*
  int sizeList[5][5] = {
    { 0x4000, 0x1000,      0,      0,      0 },
    { 0x8000, 0x2000, 0x2000, 0x8000, 0x4000 },
    { 0x8000, 0x2000, 0x2000, 0x8000, 0x4000 },
    { 0x8000, 0x2000, 0x2000, 0x8000, 0x4000 },
    { 0x8000, 0x2000, 0x2000, 0x8000, 0x4000 },
    */
  int sizeList[5][6] = {
    { 0x4000, 0x2400,      0,      0,      0,      0 },
    { 0x8000, 0x2400, 0x2000, 0x8000, 0x4000,      0 },
    { 0x8000, 0x2400, 0x2000, 0x8000, 0x4000, 0x2000 },
    { 0x8000, 0x2400, 0x2000, 0x8000, 0x4000,      0 },
    { 0x8000, 0x2400, 0x2000, 0x8000, 0x4000, 0x2000 },
    /* 99.06.02. */

  };
  /* patches in BASICROM */
  word BROMPatches[5][32] = {
    { 0x10EA,0x18,0 },				/* 60:to limit 16K	*/
    { 0 },					/* 62:do nothing	*/
    { 0x6062,0xAF,0 },				/* 64:use mode menu	*/
    { 0x601C,0x18,0x601D,0x03,0 },		/* 66:skip disk init	*/
    { 0x6062,0xAF,0 },				/* 68:use mode menu	*/
  };
  
  /*** PSG initial register states: ***/
  static byte PSGInit[16]  = { 0,0,0,0,0,0,0,0xFD,0,0,0,0,0,0,0xFF,0 };

  /*** STARTUP CODE starts here: ***/

  T=(int *)"\01\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
#ifdef LSB_FIRST
  if(*T!=1)
  {
    printf("********** This machine is high-endian. **********\n");
    printf("Take #define LSB_FIRST out and compile iP6 again.\n");
    return(0);
  }
#else
  if(*T==1)
  {
    printf("********* This machine is low-endian. **********\n");
    printf("Insert #define LSB_FIRST and compile iP6 again.\n");
    return(0);
  }
#endif

  CasStream=PrnStream=NULL;

  OpenFile(FILE_PRNT);
  OpenFile(FILE_TAPE);

  if(Verbose) printf("Allocating 8kB for empty space...");
  EmptyRAM=malloc(0x2000);

  /* allocate 64KB RAM */
  if(Verbose) printf("OK\nAllocating 64kB for RAM space...");
  if(!(RAM=malloc(0x10000))) return(0);

  if(Verbose) printf("OK\nLoading ROMs:\n");
  /* Load ROMs */

  /*
  for(K=0;K<5;K++) {
  */
  for(K=0;K<6;K++) {
  /* 99.06.02. */

    if(sizeList[P6Version][K]) {
      if(!LoadROM(ROMList[K],ROMName[P6Version][K],sizeList[P6Version][K]))
	{ PRINTFAILED;return(0); }

    } else {
      *ROMList[K] = EmptyRAM;
      /* 99.06.02. */

    };
  }
  CGROM = CGROM1;
  /* Load ExtROMs */
  if ((*Ext1Name) || (*Ext2Name)) {
    if((EXTROM=malloc(0x4000))!=NULL) {
      EXTROM1 = EXTROM; EXTROM2 = EXTROM + 0x2000;
      if (*Ext1Name)
	if(!LoadEXTROM(EXTROM1,Ext1Name,0x4000))
	  { PRINTFAILED;return(0); }
      if (*Ext2Name)
	if(!LoadEXTROM(EXTROM2,Ext2Name,0x2000))
	  { PRINTFAILED;return(0); }
    }
  } else {
    EXTROM1 = EXTROM2 = EmptyRAM;
  };

  /* patch BASICROM */
  if((PatchLevel)&&(BROMPatches[P6Version][0])) {
    if(Verbose) printf("  Patching BASICROM: ");
    for(J=0;BROMPatches[P6Version][J];) {
      if(Verbose) printf("%04X..",BROMPatches[P6Version][J]);
      P=BASICROM+BROMPatches[P6Version][J++];
      P[0]=BROMPatches[P6Version][J++];
    };
    if(Verbose) printf(".OK\n");
  };

  if(Verbose) printf("Initializing memory mappers...");

  for(J=0;J<4;J++) {RdMem[J]=BASICROM+0x2000*J;WrMem[J]=RAM+0x2000*J;};
  RdMem[2] = EXTROM1; RdMem[3] = EXTROM2;
  for(J=4;J<8;J++) {RdMem[J]=RAM+0x2000*J;WrMem[J]=RAM+0x2000*J;};
  EnWrite[0]=EnWrite[1]=0; EnWrite[2]=EnWrite[3]=1;
  VRAM=RAM;

  if(Verbose) printf("OK\nInitializing PSG, and CPU...");
  memcpy(PSG,PSGInit,sizeof(PSG));

  JoyState[0]=JoyState[1]=0xFF;
  PSGReg=0;
  EndOfFrame=1;

  if(Verbose) printf("OK\nRUNNING ROM CODE...\n");
  ResetZ80();
  
#ifdef __EMSCRIPTEN__
 printf("set main loop \n");
  emscripten_set_main_loop(Z80, 60, 1);
#else
        Z80();
#endif

  //if(Verbose) printf("EXITED at PC = %04Xh.\n",A);
  if(Verbose) printf("EXITED \n");
  return(1);
}

/****************************************************************/
/*** Free memory allocated with StartP6().                   ***/
/****************************************************************/
void TrashP6(void)
{
  if(BASICROM)  free(BASICROM);
  if(CGROM1)  free(CGROM1);
  if(CGROM5)  free(CGROM5);
  if(KANJIROM)  free(KANJIROM);
  if(VOICEROM)  free(VOICEROM);
  if(RAM)  free(RAM);
  if(EmptyRAM)  free(EmptyRAM);
  if(PrnStream&&(PrnStream!=stdout)) fclose(PrnStream);
  if(CasStream) fclose(CasStream);
}

/****************************************************************/
/*** Load a ROM data from the file name into mem.             ***/
/*** Return memory address of a ROM or 0 if failed.           ***/
/****************************************************************/
byte *LoadROM(byte **mem, char *name, int size)
{
 char path[1024];           // rom directory
 strcpy( path , "rom/");
 strcat( path , name );
  FILE *F;
  if(Verbose) printf("  Opening %s...", name);
  F=fopen(path, "rb");
  if (F) {
    if(Verbose) printf("OK\n");
    if((*mem=malloc(size))!=NULL) {
      if(Verbose) printf("    loading...");
      /* resize for CGROM60 */
      if (!strcmp(name,"CGROM60.60")) {
	size=0x1000;
      } else if (!strncmp(name,"CGROM60.",8)) {
	size=0x2000;
      }
      if(fread(*mem,1,size,F)!=size) { free(*mem); *mem=NULL; };
    };
    fclose(F);
  };
  if(Verbose) printf(*mem? MsgOK:MsgFAILED);

  /* make semi-graphic 6 for 6001 */
  // --- 2000.10.13.
  //if(!strcmp(name,"CGROM60.60")) {
  if((!strcmp(name,"CGROM60.60")) || ( !strcmp(name,"CGROM60.64")) ||
     (!strcmp(name,"CGROM60.68"))) {
  // --- 2000.10.13.
    byte *P = *mem+0x1000;
    unsigned int i, j, m1, m2;
    for(i=0; i<64; i++) {
      for(j=0; j<16; j++) {
	switch (j/4) {
          case 0: m1=0x20; m2=0x10; break;
          case 1: m1=0x08; m2=0x04; break;
          case 2: m1=0x02; m2=0x01; break;
          default: m1=m2=0;
	};
	*P++=(i&m1 ? 0xF0: 0) | (i&m2 ? 0x0F: 0);
      };
    };
  };

  /* make semi-graphic 4 for N60-BASIC */
  if (!strncmp(name,"CGROM60.",8)) {
    byte *P = *mem+0x2000;
    unsigned int i, j, m1, m2;
    for(i=0; i<16; i++) {
      for(j=0; j<16; j++) {
	switch (j/6) {
          case 0: m1=0x08; m2=0x04; break;
          case 1: m1=0x02; m2=0x01; break;
          default: m1=m2=0;
	};
	*P++=(i&m1 ? 0xF0: 0) | (i&m2 ? 0x0F: 0);
      };
    };
  };
  return(*mem);
}

/****************************************************************/
/*** Load an extension ROM data from the file name into mem.  ***/
/*** Return memory address of a ROM or 0 if failed.           ***/
/****************************************************************/
int LoadEXTROM(byte *mem, char *name, int size)
{
  FILE *F;
  size_t s=0;

  if(Verbose) printf("  Opening %s as EXTENSION ROM...", name);
  F=fopen(name, "rb");
  if (F) {
    if(Verbose) printf("OK\n");
    if(Verbose) printf("    loading...");
    s = fread(mem,1,size,F);
    if(Verbose) printf("(%04x bytes) ", s);
    fclose(F);
  };
  if(Verbose) printf((F!=NULL) ? MsgOK : MsgFAILED);
  return(F!=NULL);
}

/****************************************************************/
/*** Close(if needed) and Open tape/disk/printer file.        ***/
/****************************************************************/
void OpenFile(unsigned int num)
{
  switch(num) {
  case FILE_TAPE:
    if(CasStream) fclose(CasStream);
    if(*CasName)
      if((CasStream=fopen(CasName,"r+b")) != NULL)
	{ if(Verbose) printf("Using %s as a tape\n",CasName); }
      else 
	printf("Can't open %s as a tape\n",CasName);
    break;
  case FILE_DISK:
    break;
  case FILE_PRNT:
    if(PrnStream&&(PrnStream!=stdout)) fclose(PrnStream);
    if(!*PrnName) PrnStream=stdout;
    else
      {
	if(Verbose) printf("Redirecting printer output to %s...",PrnName);
	if(!(PrnStream=fopen(PrnName,"wb"))) PrnStream=stdout;
	if(Verbose) printf((PrnStream==stdout)? MsgFAILED:MsgOK);
      }
    break;
  }
}

/****************************************************************/
/*** Send a character to the printer.                         ***/
/****************************************************************/
void Printer(byte V) { fputc(V,PrnStream); }

/****************************************************************/
/*** Refresh screen, check keyboard and sprites. Call this    ***/
/*** function on each interrupt.                              ***/
/****************************************************************/
word Interrupt(void)
{
  /* interrupt priority (PC-6601SR) */
  /* 1.Timer     , 2.subCPU    , 3.Voice     , 4.VRTC      */
  /* 5.RS-232C   , 6.Joy Stick , 7.EXT INT   , 8.Printer   */

  if (TimerIntFlag == INTFLAG_REQ) return(INTADDR_TIMER); /* timer interrupt */
  else if (CasMode && (p6key == 0xFA) && (keyGFlag == 1))
    return(INTADDR_CMTSTOP); /* Press STOP while CMT Load or Save */
  else if ((CasMode==CAS_LOADING) && (CmtIntFlag == INTFLAG_REQ)) {
    /* CMT Loading */
    CmtIntFlag = INTFLAG_NONE;
    if(!feof(CasStream)) { /* if not EOF then Interrupt to Load 1 byte */
      CasMode=CAS_LOADBYTE;
      return(INTADDR_CMTREAD);
    } else { /* if EOF then Error */
      printf("tape file reached EOF\n");
      CasMode=CAS_NONE;
      return(INTADDR_CMTSTOP); /* Break */
    }
  } else if ((StrigIntFlag == INTFLAG_REQ) && IntSW_F3) /* if command 6 */
    return(INTADDR_STRIG);
  else if ((KeyIntFlag == INTFLAG_REQ) && IntSW_F3) /* if any key pressed */
    if (keyGFlag == 0) return(INTADDR_KEY1); /* normal key */
    else return(INTADDR_KEY2); /* special key (graphic key, etc.) */
  else /* none */
    return(INT_NONE);
}

void Patch(reg *R)
{
  printf("**********iP6: patch at PC:%4X**********\n",R->PC.W);
}

/* 画面の更新処理 */
/* Z80.cから呼ばれる */
void UpdateScreen(void)
{
  static int count = 0;
  static int UCount=1;
  
  if ((CasStream) && (count++ == 60)) {
    fpos_t pos ; // = 0;
    count = 0;
    // カセットの読み込み位置取得
    fgetpos(CasStream, &pos);
    SetTitle(pos);
  }
  if((!--UCount)||!EndOfFrame) {
    UCount=UPeriod;
    /* Refreshing screen: */
    LockSurface();              // SDL lock surface
    SCR[CRTMode1][CRTMode2 ? (CRTMode3 ? 3 : 2) : 0]();
    UnlockSurface();        // SDL Unlock surface
  }
}

/* CMT割り込みの許可の外部インタフェース */
void enableCmtInt(void)
{ if (CasMode) CmtIntFlag = INTFLAG_REQ; }

void ramdump(void)
{
  FILE *fp;
  if(!(fp = fopen("ip6core", "w")))
    { puts("can't open ip6core"); return; }
  if(fwrite(RAM, 1, 0x10000, fp)!=0x10000)
    { puts("can't write ip6core"); return; }
  fclose(fp);
  puts("wrote ip6core...OK");
}
