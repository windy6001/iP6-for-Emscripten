#include <memory.h>
#include "P6.h"

/*** PSG initial register states: ***/
static byte PSGInit[16]  = { 0,0,0,0,0,0,0,0xFD,0,0,0,0,0,0,0xFF,0 };

byte PSGReg=0;
byte PSG[16];
byte JoyState[2];

void audio_Constructor(void)
{
	JoyState[0]=JoyState[1]=0xFF;
	PSGReg=0;
	if(Verbose) printf("OK\nInitializing PSG, and CPU...");
	memcpy(PSG,PSGInit,sizeof(PSG));
}


void DoOutA0(byte Port,byte Value)
{
	switch( Port) {
		case 0xA0:  PSGReg=Value&0x0F;return; /* 8910 reg addr        */
	}
}

void DoOutA1(byte Port,byte Value)
{
	switch( Port) {
		case 0xA1:  PSG[PSGReg]=Value;        /* 8910 data            */
					PSGOut(PSGReg,Value);
					return;
	}
}


byte DoInA2(byte Port)
{
	byte Value= NORAM;
	switch(Port) {
		case 0xA2:                           /* 8910 data */
			if(PSGReg!=14) Value=(PSGReg>13? 0xff:PSG[PSGReg]);
			else
			if(PSG[15]==0xC0) Value=JoyState[0];
			else              Value=JoyState[1];
			break;
	}
	return Value;
}