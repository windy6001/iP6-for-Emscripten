#if 1
//             SDL2
//
//  modified by Windy
#include "SDL.h"
#include "P6.h"
#include "Unix.h"
#include <stdlib.h>
#define M5WIDTH 320
#define M5HEIGHT 200

    
SDL_Window *sdlWindow;   // ウインドウ
SDL_Renderer *sdlRenderer; // レンダラー
SDL_Texture *sdlTexture;	// テクスチャー
SDL_Surface * surface;   // サーフェス（オフスクリーン）
SDL_Palette palet;		// パレット情報


ColTyp BPal[16],BPal11[4],BPal12[8],BPal13[8],BPal14[4],BPal15[8],BPal53[32];

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
//int do_install, do_sync;
int scale = 2;

/** X-related definitions ************************************/
//#define KeyMask KeyPressMask|KeyReleaseMask
//#define OtherMask FocusChangeMask|ExposureMask|StructureNotifyMask
//#define MyEventMask KeyMask|OtherMask

/** Various X-related variables ******************************/
char *Dispname=NULL;
//Display *Dsp;
//Window Wnd;
//Atom DELETE_WINDOW_atom, WM_PROTOCOLS_atom;
//Colormap CMap;
//int CMapIsMine=0;
//GC DefaultGC;

//XImage *ximage;
byte *XBuf;

/* variables related to colors */
//XVisualInfo VisInfo;
//long VisInfo_mask=VisualNoMask;
int bitpix;
Bool lsbfirst;
byte Xtab[4];
//XID white,black;

//ColTyp BPal[16],BPal11[4],BPal12[8],BPal13[8],BPal14[4],BPal15[8],BPal53[32];

int Mapped; /* flags */

void InitColor(int);
/*void TrashColor(void);*/

/* size of window */
Bool wide,high;
int Width,Height; /* this is ximage size. window size is +20 */

/** Various variables and short functions ********************/

byte *Keys;
//byte Keys1[256][2],Keys2[256][2],Keys3[256][2],Keys4[256][2],
//  Keys5[256][2],Keys6[256][2],Keys7[256][2];
int scr4col = 0;
static long psec=0, pusec=0;


void LockSurface(void)
{
    SDL_LockSurface( surface);
}

void UnlockSurface(void)
{
    SDL_UnlockSurface( surface);
}


void OnBreak(int Arg) { CPURunning=0; }

void unixIdle(void)
{
 #ifndef __EMSCRIPTEN__
 SDL_Delay(10);
#endif

#if 0
  long csec, cusec;
  long l;
  struct timeval tv;

  gettimeofday(&tv, NULL);
  csec=tv.tv_sec; cusec=tv.tv_usec;
  l=(1000000.f/60.f)-((csec-psec)*1000000+(cusec-pusec));
  if(l>0) usleep(l);
  psec=csec; pusec=cusec;
  return;
#endif
}


/** Keyboard bindings ****************************************/
#include "Keydef.h"

void TrashMachine(void)
{
	SDL_Quit();
}

/** InitMachine **********************************************/
/** Allocate resources needed by Unix/X-dependent code.     **/
/*************************************************************/
int InitMachine(void)
{
  int K,L;
  int depth=32 , screen_num;
  //Window root;
  
  SetValidLine( 192);
  SetClock(4*1000000);
  
#if 0
  signal(SIGINT,OnBreak);
  signal(SIGHUP,OnBreak);
  signal(SIGQUIT,OnBreak);
  signal(SIGTERM,OnBreak);
#endif

#ifdef SOUND
  if(UseSound) InitSound();
#endif SOUND
  Width=M5WIDTH*scale;
  Height=M5HEIGHT*scale;
  
  lsbfirst =0;
  bitpix= 32;
  choosefuncs(lsbfirst,bitpix);
  /*setwidth(wide);*/
  setwidth(scale-1);


  if(Verbose)
    printf("Initializing SDL drivers:\n    SDL_Init ...");

      if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_TIMER) != 0) {
        SDL_Log("SDLを初期化できなかった: %s", SDL_GetError());
        return 1;
    }
    printf("Ok\n    SDL_CreateWindow ...");
    


    sdlWindow = SDL_CreateWindow(
        "iP6     (F12:QUIT)",                  // ウィンドウのタイトル
        SDL_WINDOWPOS_UNDEFINED,           // X座標の初期値
        SDL_WINDOWPOS_UNDEFINED,           // Y座標の初期値
        Width,                               // 幅のピクセル数
        Height,                               // 高さのピクセル数
        SDL_WINDOW_OPENGL                  // フラグ
    );
    if (sdlWindow == NULL) {
        printf("ウィンドウを生成できなかった: %s\n", SDL_GetError());
        return 0;
    }
    printf("Ok\n    SDL_CreateRGBSurface ...");
    

    surface = SDL_CreateRGBSurface(0, Width, Height, depth,  0,0,0,0);
    if (surface == NULL) {
        SDL_Log("CreateRGBSurface 失敗: %s", SDL_GetError());
        exit(1);
    }

    XBuf = (byte*)surface->pixels;		// pixelのアドレスをセット	(VMの描画プログラムで使用)
    
    printf("Ok\n    SDL_CreateRenderer ...");
    sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, SDL_RENDERER_SOFTWARE);	// レンダラー生成
    if( sdlRenderer == NULL) {
        SDL_Log("CreateRenderer 失敗: %s", SDL_GetError());
        exit(1);    	
    }
    printf("Ok\n    SDL_CreateTexture ...");
    sdlTexture = SDL_CreateTexture( sdlRenderer,				// テクスチャー生成
    													SDL_PIXELFORMAT_ARGB8888,
    													SDL_TEXTUREACCESS_STREAMING,
    													Width,Height);
    if( sdlTexture == NULL) {
        SDL_Log("CreateTexture 失敗: %s", SDL_GetError());
        printf("CreateTexture 失敗: %s", SDL_GetError());
        exit(1);    	
    }
    
    InitColor(screen_num); 		// 色の初期化
    
    SDL_LockSurface( surface);
    SDL_memset( surface->pixels , 0 , surface->h *surface->pitch);
    SDL_UnlockSurface( surface);
    
  if(Verbose & 1) printf("OK\n");
  return(1);
}






/** Keyboard *************************************************/
/** Check for keyboard events, parse them, and modify P6    **/
/** keyboard matrix.                                        **/
/*************************************************************/
void Keyboard(void)
{
  SDL_Event e;
   
  //printf("keyboard ");
  	
#if 1
  int J;

#ifdef SOUND
  //FlushSound();  /* Flush sound stream on each interrupt */
#endif
//  if(XCheckWindowEvent(Dsp,Wnd,KeyPressMask|KeyReleaseMask,&E))
   //printf("keyup %c ", e.key.keysym.sym);

  int ret= SDL_PollEvent( &e);
  if( ret==0) return;
  //printf("keyboard %04X \n" , e.type );
  
 // if( e.type == SDL_QUIT) exit(0);
  	J= e.key.keysym.sym;
	if( e.type == SDL_KEYDOWN ||  e.type == SDL_KEYUP) {
		//printf("sdl key %06X\n",J);
		byte tmp;
		switch(J) {
			case SDLK_SPACE  : tmp = STICK0_SPACE; break;
			case SDLK_LEFT   : tmp = STICK0_LEFT; break;
			case SDLK_RIGHT  : tmp = STICK0_RIGHT; break;
			case SDLK_DOWN   : tmp = STICK0_DOWN; break;
			case SDLK_UP     : tmp = STICK0_UP; break;
			case SDLK_PAUSE  : tmp = STICK0_STOP; break;
			case SDLK_LSHIFT : 
			case SDLK_RSHIFT : tmp = STICK0_SHIFT; break;
			default: tmp = 0;
		}
		if(e.type== SDL_KEYDOWN)
			stick0 |= tmp;
		else
			stick0 &= ~tmp;

		//J=XLookupKeysym((XKeyEvent *)&E,0);

		// printf("%02X \n",J);
		/* for stick,strig */
		{
		byte tmp;
		if( e.type == SDL_KEYDOWN){
		switch(J)
			{
			/*case XK_F10:
#ifdef SOUND
			StopSound(); 
#endif
			run_conf();
			ClearScr(); */
#ifdef SOUND
		ResumeSound();
#endif
				break;
			case SDLK_UP:   J= 0x111; break;
			case SDLK_DOWN: J= 0x112; break;
			case SDLK_RIGHT:J= 0x113; break;
			case SDLK_LEFT: J= 0x114; break;
			case SDLK_PAGEUP:J= 0x118; break;
			case SDLK_F1:   J= 0x11a; break;
			case SDLK_F2:   J= 0x11b; break;
			case SDLK_F3:   J= 0x11c; break;
			case SDLK_F4:   J= 0x11d; break;
			case SDLK_F5:   J= 0x11e; break;
			
#ifdef DEBUG
			case SDLK_F11: Trace=!Trace;break;
#endif
			case SDLK_F12: CPURunning=0;break;

			case SDLK_LCTRL:
			case SDLK_RCTRL:kbFlagCtrl=1;break;
			case SDLK_LALT: 
			case SDLK_RALT: kbFlagGraph=1;break;

			//case XK_Insert: J=XK_F13;break; // Ins -> F13 
			}
			if((P6Version==0)&&(J==0xFFC6)) J=0; // MODE key when 60 
		
		//J&=0xFF;
		if( J>0x140) J= 0x13f;
		if (kbFlagGraph)
			Keys = Keys7[J];
		else if (kanaMode)
			if (katakana)
				if (stick0 & STICK0_SHIFT) 
					Keys = Keys6[J];
				else 
					Keys = Keys5[J];
			else
				if (stick0 & STICK0_SHIFT) 
					Keys = Keys4[J];
				else 
					Keys = Keys3[J];
		else
			if (stick0 & STICK0_SHIFT) 
				Keys = Keys2[J];
			else 
				Keys = Keys1[J];
		keyGFlag = Keys[0]; p6key = Keys[1];

			/* control key + alphabet key */
		if ((kbFlagCtrl == 1) && (J >= SDLK_a) && (J <= SDLK_z))
			{keyGFlag = 0; p6key = J - SDLK_a + 1;}
			/*if (p6key != 0x00) IFlag = 1;*/
		if (Keys[1] != 0x00) 
			KeyIntFlag = INTFLAG_REQ;
		} else {
		if (J==SDLK_LALT || J==SDLK_RALT) kbFlagGraph=0;
		if (J==SDLK_LCTRL || J==SDLK_RCTRL) kbFlagCtrl=0;
		}
	}

/*  for(J=0;XCheckWindowEvent(Dsp,Wnd,FocusChangeMask,&E);)
    J=(E.type==FocusOut); 
  if(SaveCPU&&J)
  {
#ifdef SOUND
    StopSound();          // Silence the sound  
#endif

    while(!XCheckWindowEvent(Dsp,Wnd,FocusChangeMask,&E)&&CPURunning)
    {
      if(XCheckWindowEvent(Dsp,Wnd,ExposureMask,&E)) PutImage();
      XPeekEvent(Dsp,&E);
    }

#ifdef SOUND
    ResumeSound();        // Switch the sound back on
#endif
*/
  }
#endif
}


/* Below are the routines related to allocating and deallocating
   colors */

/* set up coltable, alind8? etc. */
void InitColor(int screen_num)
{
  SDL_Color colors[32];
  XID col;
//  XColor Color;
  register byte i,j;
  register word R,G,B,H;
  int param[16][3] = /* {R,G,B} */
    { {0,0,0},{4,3,0},{0,4,3},{3,4,0},{3,0,4},{4,0,3},{0,3,4},{3,3,3},
      {0,0,0},{4,0,0},{0,4,0},{4,4,0},{0,0,4},{4,0,4},{0,4,4},{4,4,4} };

  unsigned short trans[] = { 0x0000, 0x3fff, 0x7fff, 0xafff, 0xffff };
  //unsigned short trans[] = { 0x00, 0x3f, 0x7f, 0xaf, 0xff };
  int Pal11[ 4] = { 15, 8,10, 8 };
  int Pal12[ 8] = { 10,11,12, 9,15,14,13, 1 };
  int Pal13[ 8] = { 10,11,12, 9,15,14,13, 1 };
  int Pal14[ 4] = {  8,10, 8,15 };
  int Pal15[ 8] = {  8,13,11,10, 8,13,10,15 };
  int Pal53[32] = {  0, 4, 1, 5, 2, 6, 3, 7, 8,12, 9,13,10,14,11,15,
		    10,11,12, 9,15,14,13, 1,10,11,12, 9,15,14,13, 1 };

 for(int i=0; i<4; i++)
    Xtab[i] = i;

  for(H=0;H<2;H++)
    for(B=0;B<2;B++)
      for(G=0;G<2;G++)
        for(R=0;R<2;R++)
        {
          i=R|(G<<1)|(B<<2)|(H<<3);
          
          colors[i].r    = trans[param[i][0]]/256;
          colors[i].g   = trans[param[i][1]]/256;
          colors[i].b   = trans[param[i][2]]/256;
          //col=XAllocColor(Dsp,CMap,&Color)? Color.pixel:black;
	  if (bitpix <= 8) 
	    {
	    //BPal[i].ct_byte[0]=col;
	    BPal[i].ct_byte[0]= i;
	    }
	  else 
	    {
	     BPal[i].ct_byte[0] = colors[i].b;
	     BPal[i].ct_byte[1] = colors[i].g;   
	     BPal[i].ct_byte[2] = colors[i].r;   
/*	    for(j=0; j<4; j++) {
	        BPal[i].ct_byte[j]=((ColTyp)col).ct_byte[Xtab[j]];
	        }*/
        }
    }
   SDL_SetPaletteColors( &palet ,colors ,0,16);
   
  /* setting color list for each screen mode */
  for(i=0;i<4;i++) BPal11[i] = BPal[ Pal11[i] ];
  for(i=0;i<8;i++) BPal12[i] = BPal[ Pal12[i] ];
  for(i=0;i<8;i++) BPal13[i] = BPal[ Pal13[i] ];
  for(i=0;i<4;i++) BPal14[i] = BPal[ Pal14[i] ];
  for(i=0;i<8;i++) BPal15[i] = BPal[ Pal15[i] ];
  for(i=0;i<32;i++) BPal53[i] = BPal[ Pal53[i]];
}

/** ClearScr: clear a window *********************************/
void ClearScr()
{
 #if 0
 memset(XBuf,(byte)black,Width*Height*bitpix/8);
 #endif
}

void SetTitle(fpos_t pos)
{
#if 0
  sprintf(TitleBuf, "%s  [%ldcnt.]", Title, pos);
  SetWMHints();
#endif
}

/** PUTIMAGE: macro copying image buffer into a window ********/
void PutImage(void)
{
	
    SDL_UpdateTexture(sdlTexture, NULL, surface->pixels, surface->pitch);
    
    SDL_RenderClear( sdlRenderer );
    SDL_RenderCopy( sdlRenderer, sdlTexture, NULL, NULL);
    SDL_RenderPresent( sdlRenderer ); 
}
#endif