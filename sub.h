#include <stdio.h>

#define CAS_NONE	    0
#define CAS_SAVEBYTE	1
#define CAS_LOADING	    2
#define CAS_LOADBYTE	3
void DoOut90(byte Port , byte Value);

byte DoIn90(byte Port );
byte DoIn92(byte Port );
byte DoIn93(byte Port );

void Printer(byte V);             /* Send a character to a printer   */

extern byte CasMode;

extern char PrnName[FILENAME_MAX];    /* Printer redirect. file */
extern FILE *PrnStream;

extern char CasName[FILENAME_MAX];    /* Tape image file        */
extern FILE *CasStream;

void sub_Constructor(void);
void sub_Destructor(void);

int  GetCasPos(void);
