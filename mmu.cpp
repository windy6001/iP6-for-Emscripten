#include "mmu.h"
#include "P6.h"
#include <stdio.h>
#include <stdlib.h>		// malloc/free
#include <string.h>		// strcmp

int portF0 = 0x11;
int CGSW93 = FALSE;

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
byte *EmptyRAM;
byte *RAM;
byte EnWrite[4];

/* Load a ROM file into given pointer */
byte *LoadROM(byte **mem, char *name, int size);

int LoadEXTROM(byte *mem, char *name, int size);


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
/*** Allocate memory, load ROM images, initialize mapper, VDP ***/
/*** CPU and start the emulation. This function returns 0 in  ***/
/*** the case of failure.                                     ***/
/****************************************************************/
//int StartP6(void)
int mmu_Constructor(void)
{
	word A;
	int *T,J,K;
	byte *P;

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


	if(Verbose) printf("Allocating 8kB for empty space...");
	EmptyRAM=(byte*)malloc(0x2000);

	/* allocate 64KB RAM */
	if(Verbose) printf("OK\nAllocating 64kB for RAM space...");
	if(!(RAM=(byte*)malloc(0x10000))) return(0);

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
		if((EXTROM=(byte*)malloc(0x4000))!=NULL) {
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


	EndOfFrame=1;

	if(Verbose) printf("OK\nRUNNING ROM CODE...\n");
	ResetZ80();
	
	return(1);
}

/****************************************************************/
/*** Free memory allocated with StartP6().                   ***/
/****************************************************************/
void mmu_Destructor(void)
{
	if(BASICROM)  free(BASICROM);
	if(CGROM1)  free(CGROM1);
	if(CGROM5)  free(CGROM5);
	if(KANJIROM)  free(KANJIROM);
	if(VOICEROM)  free(VOICEROM);
	if(RAM)  free(RAM);
	if(EmptyRAM)  free(EmptyRAM);
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
    if((*mem=(byte*)malloc(size))!=NULL) {
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
    if(Verbose) printf("(%04lx bytes) ", s);
    fclose(F);
  };
  if(Verbose) printf((F!=NULL) ? MsgOK : MsgFAILED);
  return(F!=NULL);
}

/****************************************************************/
/*   DoOut93													*/
/****************************************************************/
void DoOut93(byte Port , byte Value) {
	switch(Port) {
		case 0x93: 
			switch(Value) {
				/*
				case 0x02: EndOfFrame=0; break;
				case 0x03: EndOfFrame=1; break;
				*/
				case 0x04: CGSW93 = TRUE; RdMem[3]=CGROM; break;
				case 0x05: CGSW93 = FALSE; DoOut(0xF0,portF0); break;
			}
	}
    return;
}

/****************************************************************/
/*   DoOutC2													*/
/****************************************************************/
void DoOutC2( byte Port , byte Value) {
	switch(Port) {
		case 0xC2:                           /* ROM swtich           */
			if ((Value&0x02)==0x00) 
				CurKANJIROM=KANJIROM;
			else 
				CurKANJIROM=KANJIROM+0x4000;
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
			}
		}
		/* 99.06.02. */
	return;
}

/****************************************************************/
/*   DoOutF0													*/
/****************************************************************/
void DoOutF0( byte Port , byte Value)
{
	switch( Port) {
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
		if (CGSW93) { 
			RdMem[3] = CGROM;
		}
	}
	return;
}

/****************************************************************/
/*   DoOutF1													*/
/****************************************************************/
void DoOutF1( byte Port , byte Value)
{
	switch( Port){
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
			}
	}
	return;
}

/****************************************************************/
/*   DoOutF2													*/
/****************************************************************/
void DoOutF2( byte Port , byte Value)
{
	switch( Port) {
		case 0xF2:                           /* write ram block set  */
			if(Value&0x40) {EnWrite[3]=1;WrMem[6]=RAM+0xc000;WrMem[7]=RAM+0xe000;}
			else EnWrite[3]=0;
			if(Value&0x010) {EnWrite[2]=1;WrMem[4]=RAM+0x8000;WrMem[5]=RAM+0xa000;}
			else EnWrite[2]=0;
			if(Value&0x04) {EnWrite[1]=1;WrMem[2]=RAM+0x4000;WrMem[3]=RAM+0x6000;}
			else EnWrite[1]=0;
			if(Value&0x01) {EnWrite[0]=1;WrMem[0]=RAM;WrMem[1]=RAM+0x2000;}
			else EnWrite[0]=0;
	}
	return;
}


/****************************************************************/
/*   DoInF0 													*/
/****************************************************************/
byte DoInF0( byte Port) 
{
	return portF0;
}

/****************************************************************/
/*   GetVRAM 													*/
/****************************************************************/
byte* GetRAM(void)
{
	return RAM;
}

/****************************************************************/
/*   GetCGROM1 													*/
/****************************************************************/
byte* GetCGROM1(void)
{
	return CGROM1;
}

/****************************************************************/
/*   GetCGROM5 													*/
/****************************************************************/
byte* GetCGROM5(void)
{
	return CGROM5;
}

/****************************************************************/
/*   SetCGROM 													*/
/****************************************************************/
void SetCGROM( byte *value)
{
	CGROM = value;
}

/****************************************************************/
/*   ramdump													*/
/****************************************************************/
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
