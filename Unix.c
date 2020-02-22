#if 0
/** iP6: PC-6000/6600 series emualtor ************************/
/**                                                         **/
/**                           Unix.c                        **/
/**                                                         **/
/** by ISHIOKA Hiroshi 1998-2000                            **/
/** This code is based on fMSX written by Marat Fayzullin   **/
/*************************************************************/

/** Standard Unix/X #includes ********************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>

#include "P6.h"
#include "Unix.h"
#include "Xconf.h"


/** MIT Shared Memory Extension for X ************************/
#ifdef MITSHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
XShmSegmentInfo SHMInfo;
int UseSHM=1;
/** The following is used for MITSHM-auto-detection: **/
int ShmOpcode;       /* major opcode for MIT-SHM */
int (*OldHandler)();
#endif


/** Sound-related definitions ********************************/
#ifdef SOUND
int InitSound(void);
void TrashSound(void);
void FlushSound(void);
void StopSound(void);
void ResumeSound(void);
extern int UseSound;
#endif

/****************************************************************/
/*** These dummy functions are called when writes to sound    ***/
/*** registers occur. Replace them with the actual functions  ***/
/*** generating sound.                                        ***/
/****************************************************************/
#ifndef SOUND
void PSGOut(register byte R,register byte V)  { }
void SCCOut(register byte R,register byte V)  { }
#endif

char *Title="iP6 Unix/X 0.6.4";
static char TitleBuf[256];

/** Variables to pass options to the X-drivers ***************/
int SaveCPU=1;
int do_install, do_sync;
int scale = 2;

/** X-related definitions ************************************/
#define KeyMask KeyPressMask|KeyReleaseMask
#define OtherMask FocusChangeMask|ExposureMask|StructureNotifyMask
#define MyEventMask KeyMask|OtherMask

/** Various X-related variables ******************************/
char *Dispname=NULL;
Display *Dsp;
Window Wnd;
Atom DELETE_WINDOW_atom, WM_PROTOCOLS_atom;
Colormap CMap;
int CMapIsMine=0;
GC DefaultGC;

XImage *ximage;
byte *XBuf;

/* variables related to colors */
XVisualInfo VisInfo;
long VisInfo_mask=VisualNoMask;
int bitpix;
Bool lsbfirst;
byte Xtab[4];
XID white,black;

ColTyp BPal[16],BPal11[4],BPal12[8],BPal13[8],BPal14[4],BPal15[8],BPal53[32];

int Mapped; /* flags */

void InitColor(int);
/*void TrashColor(void);*/

/* size of window */
Bool wide,high;
int Width,Height; /* this is ximage size. window size is +20 */

/** Various variables and short functions ********************/

byte *Keys;
byte Keys1[256][2],Keys2[256][2],Keys3[256][2],Keys4[256][2],
  Keys5[256][2],Keys6[256][2],Keys7[256][2];
int scr4col = 0;
static long psec=0, pusec=0;

void OnBreak(int Arg) { CPURunning=0; }

void unixIdle(void)
{
  long csec, cusec;
  long l;
  struct timeval tv;

  gettimeofday(&tv, NULL);
  csec=tv.tv_sec; cusec=tv.tv_usec;
  l=(1000000.f/60.f)-((csec-psec)*1000000+(cusec-pusec));
  if(l>0) usleep(l);
  psec=csec; pusec=cusec;
  return;
}


/** PUTIMAGE: macro copying image buffer into a window ********/
void PutImage(void)
{
#ifdef MITSHM
  if(UseSHM) 
    XShmPutImage
      (Dsp,Wnd,DefaultGC,ximage,0,0,10,10,Width,Height,False);
  else
#endif
  XPutImage(Dsp,Wnd,DefaultGC,ximage,0,0,10,10,Width,Height);
  XFlush(Dsp);
}

/*
void RefreshScreen(void)
{
  if (!Mapped) return;
  PutImage();
}
*/

/** Screen Mode Handlers [N60/N66][SCREEN MODE] **************/
void (*SCR[2][4])() = 
{
  { RefreshScr10, RefreshScr10, RefreshScr10, RefreshScr10 },
  { RefreshScr51, RefreshScr51, RefreshScr53, RefreshScr54 },
};


/** Keyboard bindings ****************************************/
#include "Keydef.h"

/** TrashMachine *********************************************/
/** Deallocate all resources taken by InitMachine().        **/
/*************************************************************/
void TrashMachine(void)
{
  if(Verbose) printf("Shutting down...\n");

  /*TrashColor();*/
  if(Dsp&&Wnd)
  {
#ifdef MITSHM
    if(UseSHM)
    {
      XShmDetach(Dsp,&SHMInfo);
      if(SHMInfo.shmaddr) shmdt(SHMInfo.shmaddr);
      if(SHMInfo.shmid>=0) shmctl(SHMInfo.shmid,IPC_RMID,0);
    }
    else
#endif MITSHM
    if(ximage) XDestroyImage(ximage);
  }
  if(CMapIsMine) XFreeColormap(Dsp, CMap);
  if(Dsp) XCloseDisplay(Dsp);

#ifdef SOUND
  StopSound();TrashSound();
#endif SOUND
}


static struct vc {
    int clasnum;
    char *name;
} visualClasses[6]=
  {
      { StaticGray, "StaticGray"},
      { GrayScale, "GrayScale"},
      { StaticColor, "StaticColor"},
      { PseudoColor, "PseudoColor"},
      { TrueColor, "TrueColor"},
      { DirectColor, "DirectColor"}
  };

void SetDepth(int x)
{
    VisInfo.depth=x;
    VisInfo_mask |= VisualDepthMask;
}

void SetScreen(int x)
{
    VisInfo.screen=x;
    VisInfo_mask |= VisualScreenMask;
}

void SetVisual(char *s)
{
    int i;
    char *vc, *sp;

    for (i=0; i<6; i++) {
        for (  vc=visualClasses[i].name, sp=s ; (*vc!=0) ; vc++, sp++ )
	    if (tolower(*vc)!=tolower(*sp))
		goto next;
	VisInfo.class=visualClasses[i].clasnum;
	VisInfo_mask |= VisualClassMask;
	return;
      next: ;
    }
    printf("%s is not a known visual class.\n",s);
}

void SetVisualId(long x)
{
    VisInfo.visualid=(VisualID) x;
    VisInfo_mask |= VisualIDMask;
}

/* Function to choose a PixMapFormat and set some variables associated
   with them.  Note that the depth comes from a visual and is
   therefore supported by the screen */
int ChooseFormat(void)
{
  int i;
  int nForm; /* # pixmapformats, depth */
  XPixmapFormatValues *flist, choice; /* list of format describers
					  and choice */
  ColTyp T;
  char *order;

  flist=XListPixmapFormats(Dsp,&nForm);
  if (!flist) return (0); 

  choice.bits_per_pixel=choice.depth=0;
  for (i=0;i<nForm;i++)
    {
      if (flist[i].depth==VisInfo.depth)
	  choice=flist[i];
    }
	     
  bitpix=choice.bits_per_pixel; 

  XFree((void *)flist);
  if (!bitpix) {
      if (Verbose &1) printf("FAILED\n"); 
      return (0);
  }

  order="";
  if (bitpix>8)
  {
    order=(ImageByteOrder(Dsp)==LSBFirst) ? "LSB first" : "MSB first";
    /* how to swap the bytes ? */
    T.ct_xid = (ImageByteOrder(Dsp)==LSBFirst)? 0x03020100L : 0x00010203L ;
    if (ImageByteOrder(Dsp)==MSBFirst)
	for (i=bitpix ; i<32 ; i+=8)
	    T.ct_xid=(T.ct_xid>>8)+((T.ct_xid&0xFF)<<24);
    /* if T were represented by the bytes 0 1 2 3 
       then no swapping would be needed.. */
   for (i=0;i<4;i++) 
       Xtab[T.ct_byte[i]]=i;
/*  Xtab[j]==i means that byte i in the representation of ColTyp used
    by the machine on which fmsx is running, corresponds to byte j in
    the representation of ColTyp in the XImage 
*/
  } 
  if (bitpix<8)
    order=(lsbfirst=(BitmapBitOrder(Dsp)==LSBFirst)) 
	? "lsb first" : "msb first"; 
  if (Verbose & 1) 
      printf("depth=%d, %d bit%s per pixel (%s)\n", VisInfo.depth,bitpix,
	     (bitpix==1)?"":"s",order);
  choosefuncs(lsbfirst,bitpix);
  /*setwidth(wide);*/
  setwidth(scale-1);

  return 1;
}

int ChooseVisual(void)
{
    int n_items,i;
    XVisualInfo *VisInfo_l;

    if (Verbose&1) printf("OK\n  Choosing Visual...");

    if (VisInfo_mask!=VisualNoMask) {
	VisInfo_l=XGetVisualInfo(Dsp, VisInfo_mask, &VisInfo, &n_items);
	if (n_items!=0) {
	    if ( (Verbose&1) && (n_items>1)) 
		printf("%d matching visuals...",n_items);
	    VisInfo=VisInfo_l[0]; /* this may not be the best match */
	    XFree(VisInfo_l);
	    goto found;
	} else 
	    if (Verbose&1)
		printf("no matching visual...");
    }

    if (Verbose&1)
	printf("using default...");

    VisInfo.screen= DefaultScreen(Dsp);
    VisInfo.visualid=DefaultVisual(Dsp, VisInfo.screen)->visualid;
    VisInfo_l=XGetVisualInfo(Dsp, VisualIDMask|VisualScreenMask, 
			     &VisInfo, &n_items);
    if (n_items!=0) {
	VisInfo=VisInfo_l[0]; /* this is probably the only match */
	XFree(VisInfo_l);
	goto found;
    }

    if (Verbose&1)
	printf("FAILED\n");
    return 0;

    found:
    for (i=6 ; i ; )
	if (visualClasses[--i].clasnum==VisInfo.class)
	    break;

    if (Verbose&1) {
	printf("\n    visualid=0x%lx: %s, ", VisInfo.visualid, 
	       (i>=0) ? visualClasses[i].name : "unknown");
    }

    return 1;
}

#ifdef MITSHM
/* Error handler, used only to trap BadAccess errors on XShmAttach() */
int Handler(Display * d, XErrorEvent * Ev)
{ 
  if ( (Ev->error_code==10) /* BadAccess */ && 
       (Ev->request_code==(unsigned char) ShmOpcode) &&
       (Ev->minor_code==1)  /* X_ShmAttach */ )
    UseSHM=0; /* X-terminal apparently remote */
  else return OldHandler(d,Ev);
  return 0;
}

/* Function that tries to XShmAttach an XImage, returns 0 in case of
faillure */
int TrySHM(int depth, int screen_num)
{
  long size;

  if(Verbose & 1)
    printf("  Using shared memory:\n    Creating image...");
  ximage=XShmCreateImage(Dsp,VisInfo.visual,depth,
                           ZPixmap,NULL,&SHMInfo,Width,Height);
  if(!ximage) return (0); 

  size=(long)ximage->bytes_per_line*ximage->height;
  if(Verbose & 1) printf("OK\n    Getting SHM info for %ld %s...",
			 (size&0x03ff) ? size : size>>10,
			 (size&0x03ff) ? "bytes" : "kB" );

  SHMInfo.shmid=shmget(IPC_PRIVATE, size, IPC_CREAT|0777);
  if(SHMInfo.shmid<0) return (0);

  if(Verbose & 1) printf("OK\n    Allocating SHM...");
  XBuf=(byte *)(ximage->data=SHMInfo.shmaddr=shmat(SHMInfo.shmid,0,0));
  if(!XBuf) return (0);

  SHMInfo.readOnly=False;
  if(Verbose & 1) printf("OK\n    Attaching SHM...");
  OldHandler=XSetErrorHandler(Handler);
  if (!XShmAttach(Dsp,&SHMInfo)) return(0);
  XSync(Dsp,False);
  XSetErrorHandler(OldHandler);
  return(UseSHM);
}
#endif

/*  Inform window manager about the window size, 
    attributes and other hints
**/
void SetWMHints(void) 
{
    XTextProperty win_name,icon_name;
    char * value[1];
    XSizeHints *size;
    XWMHints *hint;
    XClassHint *klas;
    char *argv[]= {NULL};
    int argc=0;
    
    size=XAllocSizeHints();
    hint=XAllocWMHints();
    klas=XAllocClassHint();
    
    size->flags=PSize|PMinSize|PMaxSize;
    size->min_width=size->max_width=size->base_width=Width+20;
    size->min_height=size->max_height=size->base_height=Height+20;
    
    hint->input=True;
    hint->flags=InputHint;
    
    /*value[0]=Title;*/
    value[0]=TitleBuf;
    XStringListToTextProperty(value, 1, &win_name);
    value[0]="iP6";
    XStringListToTextProperty(value, 1, &icon_name);
    
    klas->res_name="iP6";
    klas->res_class="IP6";
    
    XSetWMProperties(Dsp, Wnd, &win_name, &icon_name, argv, argc,
		     size, hint, klas);
    XFree(size); 
    XFree(hint);
    XFree(klas);

    DELETE_WINDOW_atom=XInternAtom(Dsp, "WM_DELETE_WINDOW", False);
    WM_PROTOCOLS_atom =XInternAtom(Dsp, "WM_PROTOCOLS", False);

    XChangeProperty(Dsp, Wnd, WM_PROTOCOLS_atom , XA_ATOM, 32,
		    PropModeReplace, (unsigned char*) &DELETE_WINDOW_atom,1);

}

/** InitMachine **********************************************/
/** Allocate resources needed by Unix/X-dependent code.     **/
/*************************************************************/
int InitMachine(void)
{
  int K,L;
  int depth, screen_num;
  Window root;

  signal(SIGINT,OnBreak);
  signal(SIGHUP,OnBreak);
  signal(SIGQUIT,OnBreak);
  signal(SIGTERM,OnBreak);

#ifdef SOUND
  if(UseSound) InitSound();
#endif SOUND
  Width=M5WIDTH*scale;
  Height=M5HEIGHT*scale;
  if(Verbose)
    printf("Initializing Unix/X drivers:\n  Opening display...");
  Dsp=XOpenDisplay(Dispname);

  if(!Dsp) { if(Verbose) printf("FAILED\n");return(0); }
  if (do_sync) XSynchronize(Dsp, True);

  if (!ChooseVisual()) 
      return 0;
  screen_num=VisInfo.screen;  depth=VisInfo.depth;
  root=RootWindow(Dsp, screen_num);

  if (!ChooseFormat()) 
      return 0;

  white=XWhitePixel(Dsp,screen_num);
  black=XBlackPixel(Dsp,screen_num);
  DefaultGC=DefaultGC(Dsp,screen_num);
  if ( do_install 
       ||(VisInfo.visualid != DefaultVisual(Dsp,screen_num)->visualid)) {
      CMap=XCreateColormap(Dsp, root, VisInfo.visual, AllocNone);
      CMapIsMine=1;
  } 
  else CMap=DefaultColormap(Dsp,screen_num);

  if (Verbose & 1) 
      printf("  Using %s colormap (0x%lx)\n", 
	     CMapIsMine?"private":"default", CMap);

  {
    XSetWindowAttributes wat;
    
    if(Verbose & 1) printf("  Opening window...");
    
    wat.colormap=CMap;
    wat.event_mask=MyEventMask;
    wat.background_pixel=0;
/*    wat.background_pixmap=None; */
    wat.border_pixel=0;
/*    wat.border_pixmap=None; */

    Wnd=XCreateWindow(Dsp,root,0,0,Width+20,Height+20,0,depth,InputOutput,
		      VisInfo.visual, 
		      CWColormap|CWEventMask|CWBackPixel|CWBorderPixel, &wat);
  }
  strcpy(TitleBuf, Title); /* added */
  SetWMHints();
  if(!Wnd) { if(Verbose & 1) printf("FAILED\n");return(0); }
  if(Verbose & 1) printf("OK\n");

  InitColor(screen_num); 

  XMapRaised(Dsp,Wnd);
  XClearWindow(Dsp,Wnd);

#ifdef MITSHM
  if(UseSHM) /* check whether Dsp supports MITSHM */
    UseSHM=XQueryExtension(Dsp,"MIT-SHM",&ShmOpcode,&K,&L);
  if(UseSHM)
  { if(!(UseSHM=TrySHM(depth, screen_num))) /* Dsp might still be remote */ 
    {
      if(SHMInfo.shmaddr) shmdt(SHMInfo.shmaddr);
      if(SHMInfo.shmid>=0) shmctl(SHMInfo.shmid,IPC_RMID,0);
      if(ximage) XDestroyImage(ximage);
      if(Verbose & 1) printf("FAILED\n");
    }
  } 
  if (!UseSHM)
#endif
  {
    long size;
    
    size=(long)sizeof(byte)*Height*Width*bitpix/8;
    if(Verbose & 1) printf("  Allocating %ld %s RAM for image...",
			   (size&0x03ff) ? size : size>>10,
			   (size&0x03ff) ? "bytes" : "kB");
    XBuf=(byte *)malloc(size);
    if(!XBuf) { if(Verbose & 1) printf("FAILED\n");return(0); }

    if(Verbose & 1) printf("OK\n  Creating image...");
    ximage=XCreateImage(Dsp,VisInfo.visual,depth,
                        ZPixmap,0,XBuf,Width,Height,8,0);
    if(!ximage) { if(Verbose & 1) printf("FAILED\n");return(0); }
  }
  if(Verbose & 1) printf("OK\n");

  build_conf();
  {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    psec=tv.tv_sec;pusec=tv.tv_usec;
  }
  return(1);
}


/** Keyboard *************************************************/
/** Check for keyboard events, parse them, and modify P6    **/
/** keyboard matrix.                                        **/
/*************************************************************/
void Keyboard(void)
{
  XEvent E;
  int J;

#ifdef SOUND
  FlushSound();  /* Flush sound stream on each interrupt */
#endif
  if(XCheckWindowEvent(Dsp,Wnd,KeyPressMask|KeyReleaseMask,&E))
  {
    J=XLookupKeysym((XKeyEvent *)&E,0);
    /* for stick,strig */
    {
      byte tmp;
      switch(J) {
        case XK_space  : tmp = STICK0_SPACE; break;
	case XK_Left   : tmp = STICK0_LEFT; break;
	case XK_Right  : tmp = STICK0_RIGHT; break;
	case XK_Down   : tmp = STICK0_DOWN; break;
	case XK_Up     : tmp = STICK0_UP; break;
	case XK_Pause  : tmp = STICK0_STOP; break;
	case XK_Shift_L: 
	case XK_Shift_R: tmp = STICK0_SHIFT; break;
	default: tmp = 0;
      }
      if(E.type==KeyPress) stick0 |= tmp;
      else	           stick0 &= ~tmp;
    }
    /* end of for stick,strig */
    if(E.type==KeyPress)
    {
      switch(J)
      {
        case XK_F10:
#ifdef SOUND
	  StopSound();          /* Silence the sound        */
#endif
	  run_conf();
	  ClearScr();
#ifdef SOUND
	  ResumeSound();        /* Switch the sound back on */
#endif
	  break;
#ifdef DEBUG
        case XK_F11: Trace=!Trace;break;
#endif
        case XK_F12: CPURunning=0;break;

        case XK_Control_L: kbFlagCtrl=1;break;
        case XK_Alt_L: kbFlagGraph=1;break;

        case XK_Insert: J=XK_F13;break; /* Ins -> F13 */
      }
      if((P6Version==0)&&(J==0xFFC6)) J=0; /* MODE key when 60 */
      J&=0xFF;
      if (kbFlagGraph)
	Keys = Keys7[J];
      else if (kanaMode)
	if (katakana)
	  if (stick0 & STICK0_SHIFT) Keys = Keys6[J];
	  else Keys = Keys5[J];
	else
	  if (stick0 & STICK0_SHIFT) Keys = Keys4[J];
	  else Keys = Keys3[J];
      else
	if (stick0 & STICK0_SHIFT) Keys = Keys2[J];
	else Keys = Keys1[J];
      keyGFlag = Keys[0]; p6key = Keys[1];

      /* control key + alphabet key */
      if ((kbFlagCtrl == 1) && (J >= XK_a) && (J <= XK_z))
	{keyGFlag = 0; p6key = J - XK_a + 1;}
      /*if (p6key != 0x00) IFlag = 1;*/
      if (Keys[1] != 0x00) KeyIntFlag = INTFLAG_REQ;
    } else {
      if (J==XK_Alt_L) kbFlagGraph=0;
      if (J==XK_Control_L) kbFlagCtrl=0;
    }
  }

  for(J=0;XCheckWindowEvent(Dsp,Wnd,FocusChangeMask,&E);)
    J=(E.type==FocusOut); 
  if(SaveCPU&&J)
  {
#ifdef SOUND
    StopSound();          /* Silence the sound        */
#endif

    while(!XCheckWindowEvent(Dsp,Wnd,FocusChangeMask,&E)&&CPURunning)
    {
      if(XCheckWindowEvent(Dsp,Wnd,ExposureMask,&E)) PutImage();
      XPeekEvent(Dsp,&E);
    }

#ifdef SOUND
    ResumeSound();        /* Switch the sound back on */
#endif
  }
}


/* Below are the routines related to allocating and deallocating
   colors */

/* set up coltable, alind8? etc. */
void InitColor(int screen_num)
{
  XID col;
  XColor Color;
  register byte i,j;
  register word R,G,B,H;
  int param[16][3] = /* {R,G,B} */
    { {0,0,0},{4,3,0},{0,4,3},{3,4,0},{3,0,4},{4,0,3},{0,3,4},{3,3,3},
      {0,0,0},{4,0,0},{0,4,0},{4,4,0},{0,0,4},{4,0,4},{0,4,4},{4,4,4} };
    /*
    { {0,1,0},{4,3,0},{0,4,3},{3,4,0},{3,0,4},{4,0,3},{0,3,4},{3,3,3},
      {0,1,0},{4,0,0},{0,4,0},{4,4,0},{0,0,4},{4,0,4},{0,4,4},{1,4,1} };
      */
  unsigned short trans[] = { 0x0000, 0x3fff, 0x7fff, 0xafff, 0xffff };
  int Pal11[ 4] = { 15, 8,10, 8 };
  int Pal12[ 8] = { 10,11,12, 9,15,14,13, 1 };
  int Pal13[ 8] = { 10,11,12, 9,15,14,13, 1 };
  int Pal14[ 4] = {  8,10, 8,15 };
  int Pal15[ 8] = {  8,13,11,10, 8,13,10,15 };
  int Pal53[32] = {  0, 4, 1, 5, 2, 6, 3, 7, 8,12, 9,13,10,14,11,15,
		    10,11,12, 9,15,14,13, 1,10,11,12, 9,15,14,13, 1 };

  for(H=0;H<2;H++)
    for(B=0;B<2;B++)
      for(G=0;G<2;G++)
        for(R=0;R<2;R++)
        {
          Color.flags=DoRed|DoGreen|DoBlue;
          i=R|(G<<1)|(B<<2)|(H<<3);
          Color.red=trans[param[i][0]];
          Color.green=trans[param[i][1]];
          Color.blue=trans[param[i][2]];
          col=XAllocColor(Dsp,CMap,&Color)? Color.pixel:black;
	  if (bitpix <= 8) 
	    BPal[i].ct_byte[0]=col;
	  else 
	    for(j=0; j<4; j++)
	      BPal[i].ct_byte[j]=((ColTyp)col).ct_byte[Xtab[j]];
        }
  /* setting color list for each screen mode */
  for(i=0;i<4;i++) BPal11[i] = BPal[Pal11[i]];
  for(i=0;i<8;i++) BPal12[i] = BPal[Pal12[i]];
  for(i=0;i<8;i++) BPal13[i] = BPal[Pal13[i]];
  for(i=0;i<4;i++) BPal14[i] = BPal[Pal14[i]];
  for(i=0;i<8;i++) BPal15[i] = BPal[Pal15[i]];
  for(i=0;i<32;i++) BPal53[i] = BPal[Pal53[i]];
}

/** ClearScr: clear a window *********************************/
void ClearScr()
{
  memset(XBuf,(byte)black,Width*Height*bitpix/8);
}

void SetTitle(fpos_t pos)
{
  sprintf(TitleBuf, "%s  [%ldcnt.]", Title, pos);
  SetWMHints();
}
#endif

