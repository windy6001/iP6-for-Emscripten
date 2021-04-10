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
#include "mmu.h"
#include "sub.h"
#include "crtc.h"
#include "audio.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif





int P6Version;
int  PatchLevel = 1;
int portF3 = 0;
int portF6 = 0;
int portF7 = 0x06;		/* 初期値は0x06とする (PC-6001対応) */

byte Verbose = 1;



int Code16Count = 0;


char DskName[FILENAME_MAX] = "";    /* Disk image file        */
FILE *DskStream  = NULL;


int InitP6(void)
{
	int ret;
	ret = mmu_Constructor();
	sub_Constructor();
	audio_Constructor();
	return ret;
}

/****************************************************************/
/*** Free memory allocated with StartP6().                   ***/
/****************************************************************/
void TrashP6(void)
{
	mmu_Destructor();
	sub_Destructor();
}

/****************************************************************/
/*** Write number into given IO port.                         ***/
/****************************************************************/
void DoOut(register byte Port,register byte Value)
{
	if (Verbose&0x02) printf("--- DoOut : %02X, %02X ---\n", Port, Value);
	switch(Port) {
		case 0x80: 	return;
		case 0x81: 	return;                   /* 8251 status          */
		case 0x90:  DoOut90(Port , Value);    /* subCPU               */
					return;
		case 0x93:  DoOut93(Port , Value); 
					return;
		case 0xA0:	DoOutA0(Port, Value);	/* 8910 reg addr        */
					return;
		case 0xA1:	DoOutA1(Port, Value);	/* 8910 data            */
					return;

		case 0xA3: 	return;                   /* 8910 inactive ?      */

		case 0xB0:  DoOutB0(Port ,Value);
					return;
		case 0xC0:  DoOutC0(Port ,Value);	 /* CSS                  */
					return;
		case 0xC1:  DoOutC1(Port ,Value);	 /* CRT controller mode  */
					return;

		case 0xC3: return;                   /* C2H in/out switch    */
		case 0xD1: return;                   /* disk command,data    */
		case 0xD3: return;                   /* disk signal          */
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
		case 0xF8: 	return;                   /* CG access control    */
	}
	DoOut93(Port ,Value);
	DoOutC2(Port ,Value);
	DoOutF0(Port ,Value);
	DoOutF1(Port ,Value);
	DoOutF2(Port ,Value);
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
		case 0x90: Value= DoIn90(Port);break;
		case 0x92: Value= DoIn92(Port);break;
		case 0x93: Value= DoIn93(Port);break;
		case 0xA2: Value= DoInA2(Port);break;                          /* 8910 data */
		case 0xC0: Value=NORAM;break;        /* pr busy,rs carrier   */
		case 0xC2: Value=NORAM;break;        /* ROM switch           */
		case 0xD0: Value=NORAM;break;        /* disk data            */
		case 0xD2: Value=NORAM;break;        /* disk signal          */
		case 0xE0: Value=0x40;break;         /* uPD7752              */
		case 0xF0: Value=DoInF0( Port);break;       /* read block set       */
		case 0xF3: Value=portF3;break;
		case 0xF6: Value=portF6;break;
		case 0xF7: Value=portF7;break;
		default:   Value=NORAM;break;        /* If no port, ret FFh  */
	}
	if (Verbose&0x02) printf("--- DoIn  : %02X, %02X ---\n", Port, Value);
	return(Value);
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



void Patch(reg *R)
{
  printf("**********iP6: patch at PC:%4X**********\n",R->PC.W);
}


/* CMT$B3d$j9~$_$N5v2D$N30It%$%s%?%U%'!<%9(B */
void enableCmtInt(void)
{ 
	if (CasMode) {
		CmtIntFlag = INTFLAG_REQ; 
	}
}

