#include "P6.h"
#include "sub.h"

byte CasMode = CAS_NONE;

char PrnName[FILENAME_MAX] = "";    /* Printer redirect. file */
FILE *PrnStream  = NULL;

char CasName[FILENAME_MAX] = "";    /* Tape image file        */
FILE *CasStream  = NULL;


byte p6key = 0;
byte stick0 = 0;
byte keyGFlag = 0;
byte kanaMode = 0;
byte katakana = 0;
byte kbFlagGraph = 0;
byte kbFlagCtrl = 0;

void sub_Constructor(void)
{
	CasStream=PrnStream=NULL;

	OpenFile(FILE_PRNT);
	OpenFile(FILE_TAPE);
}

void sub_Destructor(void) 
{
	if(PrnStream&&(PrnStream!=stdout)) fclose(PrnStream);
	if(CasStream) fclose(CasStream);
}


void DoOut90(byte Port , byte Value)
{
    switch(Port) {
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
		case 0x91: 	Printer(~Value); return;  /* printer data         */
		case 0x92: 	return;                   /* printer,CRT,CG,sub   */
    }
}

byte DoIn90(byte Port)
{
	byte Value=NORAM;
    switch(Port) {
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
    }
 return Value;
}

byte DoIn92(byte Port )
{
	byte Value;
    switch(Port) {
		case 0x92: Value=NORAM;break;        /* printer,CRT,CG,sub   */
    }
    return Value;
}

byte DoIn93(byte Port )
{
	byte Value;
    switch(Port) {
		case 0x93: Value=NORAM;break;        /* 8255 mode, portC     */
    }
    return Value;
}

/****************************************************************/
/*** Send a character to the printer.                         ***/
/****************************************************************/
void Printer(byte V) 
{ 
    fputc(V,PrnStream); 
}

int  GetCasPos(void)
{
	int ret;
	int Value = -1;

	if( CasStream) {
		fpos_t pos;
		ret = fgetpos(CasStream, &pos);
		if( ret ==0) {
			Value = pos;
			}
	}
    return Value;
}