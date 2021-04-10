/** iP6: PC-6000/6600 series emualtor ************************/
/**                                                         **/
/**                           P6.h                          **/
/**                                                         **/
/** by ISHIOKA Hiroshi 1998,1999                            **/
/** This code is based on fMSX written by Marat Fayzullin   **/
/*************************************************************/

#include <stdio.h>

#include "Z80.h"            /* Z80 emulation declarations    */

/* #define UNIX  */         /* Compile iP6 for for Unix/X    */
/* #define MSDOS */         /* Compile iP6 for MSDOS/VGA     */
/* #define MITSHM */        /* Use MIT SHM extensions for X  */

#ifndef UNIX
#undef MITSHM
#endif

#define NORAM     0xFF      /* Byte to be returned from      */
                            /* non-existing pages and ports  */
#define MsgOK       "OK\n"
#define MsgFAILED   "FAILED\n"
#define PRINTOK     if(Verbose) printf(MsgOK)
#define PRINTFAILED if(Verbose) printf(MsgFAILED)

/******** Variables used to control emulator behavior ********/
extern byte Verbose;                  /* Debug msgs ON/OFF   */
extern int P6Version;            /* 0=60,1=62,2=64,3=66,4=68 */
/*************************************************************/
//extern byte *BASICROM;
//extern byte *VOICEROM;
//extern byte *KANJIROM;
//extern byte *CurKANJIROM;
/*
extern byte *ROM2;
*/
//extern byte *SYSROM2;
/* 99.06.02. */

extern byte *CGROM;
//extern byte *EXTROM;
//extern byte *EXTROM1;
//extern byte *EXTROM2;
//extern char Ext1Name[FILENAME_MAX];    /* Extension ROM 1 file */
//extern char Ext2Name[FILENAME_MAX];    /* Extension ROM 2 file */
//extern byte *RdMem[8];
//extern byte *WrMem[8];
extern byte *VRAM;
//extern byte *EmptyRAM;
//extern byte *RAM;
//extern byte EnWrite[4];
//extern byte CRTMode;

//extern byte PSGReg;
//extern byte PSG[16];
//extern byte JoyState[2];
extern byte CSS1,CSS2,CSS3;
extern byte EndOfFrame;               /* 1 when end of frame */

//extern byte UPeriod;   /* Number of interrupts/screen update */
extern byte p6key;
extern byte stick0;
extern byte timerInt;
extern byte keyGFlag;
extern byte kanaMode;
extern byte katakana;
extern byte kbFlagGraph;
extern byte kbFlagCtrl;
extern byte TimerSW;
extern byte TimerSW_F3;
//extern byte next90;

/*extern char *PrnName;*/                 /* Printer redir. file */
/*extern char *CasName;*/                 /* Tape image file     */
extern char PrnName[FILENAME_MAX];    /* Printer redir. file */
extern char DskName[FILENAME_MAX];    /* Disk image file     */
extern char CasName[FILENAME_MAX];    /* Tape image file     */
extern int  PatchLevel;
extern int  scr4col;

#define FILE_TAPE 0
#define FILE_DISK 1
#define FILE_PRNT 2

#define STICK0_SPACE 0x80
#define STICK0_LEFT  0x20
#define STICK0_RIGHT 0x10
#define STICK0_DOWN  0x08
#define STICK0_UP    0x04
#define STICK0_STOP  0x02
#define STICK0_SHIFT 0x01

void Keyboard(void);
int InitScrF(void);
void ClearScr(void);

void PSGOut(byte R,byte V);

void OpenFile(unsigned int num);

/****************************************************************/
/*** Allocate memory, load ROM images, initialize mapper, VDP ***/
/*** CPU and start the emulation. This function returns 0 in  ***/
/*** the case of failure.                                     ***/
/****************************************************************/
int StartP6(void);
int InitP6(void);


/****************************************************************/
/*** Free memory allocated by StartP6().                     ***/
/****************************************************************/
void TrashP6(void);

/****************************************************************/
/*** Allocate resources needed by the machine-dependent code. ***/
/************************************** TO BE WRITTEN BY USER ***/
int InitMachine(void);

/****************************************************************/
/*** Deallocate all resources taken by InitMachine().         ***/
/************************************** TO BE WRITTEN BY USER ***/
void TrashMachine(void);

void UpdateScreen(void);
void enableCmtInt(void);
void SetTitle(fpos_t pos);
