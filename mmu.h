#include "Z80.h"

extern int P6Version;
byte M_RDMEM(register word A);
void M_WRMEM(register word A,register byte V);
byte *LoadROM(byte **mem, char *name, int size);
int LoadEXTROM(byte *mem, char *name, int size);

void DoOut93(byte Port , byte Value);
void DoOutC2( byte Port , byte Value);
void DoOutF0( byte Port , byte Value);
void DoOutF1( byte Port , byte Value);
void DoOutF2( byte Port , byte Value);
byte DoInF0( byte Port);

byte* GetRAM(void);
byte* GetCGROM1(void);
byte* GetCGROM5(void);
void  SetCGROM(byte *value);

void ramdump(void);

int mmu_Constructor(void);
void mmu_Destructor(void);




