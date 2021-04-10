/** Z80: portable Z80 emulator *******************************/
/**                                                         **/
/**                           Z80.h                         **/
/**                                                         **/
/** This file contains declarations relevant to emulation   **/
/** of Z80 CPU.                                             **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-1998                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/   
/**     changes to this file.                               **/
/*************************************************************/
#ifndef Z80_H
#define Z80_H

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define INTFLAG_NONE	0
#define INTFLAG_REQ	1
#define INTFLAG_READY	2
#define INTFLAG_EXEC	3

#define INTADDR_KEY1	0x02
#define INTADDR_RS232C	0x04
#define INTADDR_TIMER	portF7
#define INTADDR_CMTREAD	0x08
#define INTADDR_CMTSTOP	0x0E
#define INTADDR_CMTERR	0x12
#define INTADDR_KEY2	0x14
#define INTADDR_STRIG	0x16

#ifndef GLOBAL
#define EXTERN extern
#endif

extern int IntSW_F3;
EXTERN int TimerIntFlag;
EXTERN int CmtIntFlag;
EXTERN int StrigIntFlag;
EXTERN int KeyIntFlag;

#define INTERRUPTS             /* Compile interrupts code    */

                               /* Compilation options:       */
/* #define DEBUG */            /* Compile debugging version  */
/* #define LSB_FIRST */        /* Compile for low-endian CPU */
/* #define MSB_FIRST */        /* Compile for hi-endian CPU  */

                               /* LoopZ80() may return:      */
                               /* Interrupt() may return:    */
#define INT_IRQ     0x0038     /* Standard RST 38h interrupt */
#define INT_NMI     0x0066     /* Non-maskable interrupt     */
#define INT_NONE    0xFFFF     /* No interrupt required      */
#define INT_QUIT    0xFFFE     /* Exit the emulation         */

                               /* Bits in Z80 F register:    */
#define S_FLAG      0x80       /* 1: Result negative         */
#define Z_FLAG      0x40       /* 1: Result is zero          */
#define H_FLAG      0x10       /* 1: Halfcarry/Halfborrow    */
#define P_FLAG      0x04       /* 1: Result is even          */
#define V_FLAG      0x04       /* 1: Overflow occured        */
#define N_FLAG      0x02       /* 1: Subtraction occured     */
#define C_FLAG      0x01       /* 1: Carry/Borrow occured    */

/** Simple Datatypes *****************************************/
/** NOTICE: sizeof(byte)=1 and sizeof(word)=2               **/
/*************************************************************/
typedef unsigned char byte;
typedef unsigned short word;
typedef signed char offset;

/** Structured Datatypes *************************************/
/** NOTICE: #define LSB_FIRST for machines where least      **/
/**         signifcant byte goes first.                     **/
/*************************************************************/
typedef union
{
#ifdef LSB_FIRST
  struct { byte l,h; } B;
#else
  struct { byte h,l; } B;
#endif
  word W;
} pair;

typedef struct
{
  pair AF,BC,DE,HL,IX,IY,PC,SP;       /* Main registers      */
  pair AF1,BC1,DE1,HL1;               /* Shadow registers    */
  byte IFF,I;                         /* Interrupt registers */
} reg;

/*** Interrupts *******************************************/
/*** Interrupt-related variables.                       ***/
/**********************************************************/
#ifdef INTERRUPTS
extern byte IFlag;    /* If IFlag==1, generate interrupt and set to 0   */
#endif

/*** Trace and Trap ***************************************/         
/*** Switches to turn tracing on and off in DEBUG mode. ***/
/**********************************************************/
#ifdef DEBUG
extern byte Trace;  /* Tracing is on if Trace==1  */
extern word Trap;   /* When PC==Trap, set Trace=1 */
#endif

/*** TrapBadOps *******************************************/
/*** When 1, print warnings of illegal Z80 instructions.***/
/**********************************************************/
extern byte TrapBadOps;

/*** CPURunning *******************************************/
/*** When 0, execution terminates.                      ***/
/**********************************************************/
extern byte CPURunning;

/*** Reset Z80 registers: *********************************/
/*** This function can be used to reset the register    ***/
/*** file before starting execution with Z80(). It sets ***/
/*** the registers to their initial values.             ***/
/**********************************************************/
void ResetZ80(void);

/*** Interpret Z80 code: **********************************/
/*** Registers have initial values from Regs. PC value  ***/
/*** at which emulation stopped is returned by this     ***/
/*** function.                                          ***/
/**********************************************************/
word Z80(void);

/*** Memory access: ***************************************/
/*** These functions are called when read or write to   ***/
/*** RAM occurs. They perform actual reads abd writes.  ***/
/**********************************************************/
void M_WRMEM(word A,byte V);
byte M_RDMEM(word A);

/*** Input/Output values from/to ports: *******************/
/*** These are called on IN and OUT commands and should ***/
/*** supplied by machine-dependent part of the code.    ***/
/**********************************************************/
byte DoIn(byte Port);
void DoOut(byte Port,byte Value);

/**********************************************************/
/*** Emulate BIOS calls. This function is called on the ***/
/*** ED FE instruction to emulate disk/tape access, etc.***/
/*** Replace it with an empty macro for now patching.   ***/
/**********************************************************/
void Patch(reg *R);

#ifdef DEBUG
/*** Single-step debugger *********************************/
/*** This function should exist if DEBUG is #defined.   ***/
/*** If Trace==1 it is called after each command        ***/
/*** executed by the CPU and given a pointer to the     ***/
/*** register file.                                     ***/
/**********************************************************/
byte Debug(reg *R);
#endif

#ifdef INTERRUPTS
/*** Interrupt handler ************************************/
/*** This function should exist if INTERRUPTS is        ***/
/*** #defined. It is called on each attempted interrupt ***/
/*** and should return an interrupt address to proceed  ***/
/*** with interrupt or 0xFFFF to continue execution.    ***/ 
/**********************************************************/
word Interrupt(void);
#endif

void SetTimerIntClock(long pow);
void SetValidLine(long Line);
void SetClock(long clock);

#endif /* Z80_H */
