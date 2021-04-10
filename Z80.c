/** Z80: portable Z80 emulator *******************************/
/**                                                         **/
/**                           Z80.c                         **/
/**                                                         **/
/** This file contains implementation for Z80 CPU. It can   **/
/** be used separately to emulate any Z80-based machine.    **/
/** In this case you will need: Z80.c, Z80.h, PTable.h,     **/
/** Codes.h, CodesED.h, CodesCB.h, CodesXX.h, CodesXCB.h.   **/
/** Don't forget to write Patch(), M_RDMEM(), and M_WRMEM() **/
/** functions to accomodate emulated machine architecture.  **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-1998                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/   
/**     changes to this file.                               **/
/*************************************************************/

#include <stdio.h>
#include "P6.h"
#include "Tables.h"
#include "cycles.h"
#include "sub.h"

//#include <SDL.h>

/*** Registers ***********************************************/
/*** Z80 registers and running flag.                       ***/
/*************************************************************/
static reg R;
byte CPURunning;

static long ClockCount=0;          /* Variable used to count CPU cycles   */

static long z80_clock = 0;
static long TimerInt_Clock = 0;
static long BaseTimerClock = 0;
static long CmtInt_Clock = 0;

static int TintClockRemain;		// タイマ割り込み監視
static int CmtClockRemain;		// CMT割り込み監視
static int Valid_Line = 0;
static long TClock = 0;			// 1秒間に実行したクロック数

int TimerIntFlag = INTFLAG_NONE;
int CmtIntFlag = INTFLAG_NONE;
int StrigIntFlag = INTFLAG_NONE;
int KeyIntFlag = INTFLAG_NONE;

word save_intr=0xffff;
int IntSW_F3 = 1;

/*** Interrupts **********************************************/
/*** Interrupt-related variables.                          ***/
/*************************************************************/
#ifdef INTERRUPTS
byte IFlag = 0;       /* If IFlag==1, gen. int. and set to 0 */
#endif

/*** Trace and Trap ******************************************/
/*** Switches to turn tracing on and off in DEBUG mode.    ***/
/*************************************************************/
#ifdef DEBUG
byte Trace=0;       /* Tracing is on if Trace==1  */
word Trap=0xFFFF;   /* When PC==Trap, set Trace=1 */
#endif

/*** TrapBadOps **********************************************/
/*** When 1, print warnings of illegal Z80 instructions.   ***/
/*************************************************************/
byte TrapBadOps=0;


#define S(Fl)        R.AF.B.l|=Fl
#define R(Fl)        R.AF.B.l&=~(Fl)
#define FLAGS(Rg,Fl) R.AF.B.l=Fl|ZSTable[Rg]

#define M_RLC(Rg)      \
  R.AF.B.l=Rg>>7;Rg=(Rg<<1)|R.AF.B.l;R.AF.B.l|=PZSTable[Rg]
#define M_RRC(Rg)      \
  R.AF.B.l=Rg&0x01;Rg=(Rg>>1)|(R.AF.B.l<<7);R.AF.B.l|=PZSTable[Rg]
#define M_RL(Rg)       \
  if(Rg&0x80)          \
  {                    \
    Rg=(Rg<<1)|(R.AF.B.l&C_FLAG); \
    R.AF.B.l=PZSTable[Rg]|C_FLAG; \
  }                    \
  else                 \
  {                    \
    Rg=(Rg<<1)|(R.AF.B.l&C_FLAG); \
    R.AF.B.l=PZSTable[Rg];        \
  }
#define M_RR(Rg)       \
  if(Rg&0x01)          \
  {                    \
    Rg=(Rg>>1)|(R.AF.B.l<<7);     \
    R.AF.B.l=PZSTable[Rg]|C_FLAG; \
  }                    \
  else                 \
  {                    \
    Rg=(Rg>>1)|(R.AF.B.l<<7);     \
    R.AF.B.l=PZSTable[Rg];        \
  }
  
#define M_SLA(Rg)      \
  R.AF.B.l=Rg>>7;Rg<<=1;R.AF.B.l|=PZSTable[Rg]
#define M_SRA(Rg)      \
  R.AF.B.l=Rg&C_FLAG;Rg=(Rg>>1)|(Rg&0x80);R.AF.B.l|=PZSTable[Rg]

#define M_SLL(Rg)      \
  R.AF.B.l=Rg>>7;Rg=(Rg<<1)|0x01;R.AF.B.l|=PZSTable[Rg]
#define M_SRL(Rg)      \
  R.AF.B.l=Rg&0x01;Rg>>=1;R.AF.B.l|=PZSTable[Rg]

#define M_BIT(Bit,Rg)  \
  R.AF.B.l=(R.AF.B.l&~(N_FLAG|Z_FLAG))|H_FLAG|(Rg&(1<<Bit)? 0:Z_FLAG)

#define M_SET(Bit,Rg) Rg|=1<<Bit
#define M_RES(Bit,Rg) Rg&=~(1<<Bit)

#define M_POP(Rg)      \
  R.Rg.B.l=M_RDMEM(R.SP.W++);R.Rg.B.h=M_RDMEM(R.SP.W++)
#define M_PUSH(Rg)     \
  M_WRMEM(--R.SP.W,R.Rg.B.h);M_WRMEM(--R.SP.W,R.Rg.B.l)

#define M_CALL         \
  J.B.l=M_RDMEM(R.PC.W++);J.B.h=M_RDMEM(R.PC.W++);       \
  M_WRMEM(--R.SP.W,R.PC.B.h);M_WRMEM(--R.SP.W,R.PC.B.l); \
  R.PC.W=J.W

#define M_JP  J.B.l=M_RDMEM(R.PC.W++);J.B.h=M_RDMEM(R.PC.W);R.PC.W=J.W
#define M_JR  R.PC.W+=(offset)M_RDMEM(R.PC.W)+1
#define M_RET R.PC.B.l=M_RDMEM(R.SP.W++);R.PC.B.h=M_RDMEM(R.SP.W++)

#define M_RST(Ad)      \
  M_WRMEM(--R.SP.W,R.PC.B.h);M_WRMEM(--R.SP.W,R.PC.B.l);R.PC.W=Ad

#define M_LDWORD(Rg)   \
  R.Rg.B.l=M_RDMEM(R.PC.W++);R.Rg.B.h=M_RDMEM(R.PC.W++)

#define M_ADD(Rg)      \
  J.W=R.AF.B.h+Rg;     \
  R.AF.B.l=            \
    (~(R.AF.B.h^Rg)&(Rg^J.B.l)&0x80? V_FLAG:0)| \
    J.B.h|ZSTable[J.B.l]|                       \
    ((R.AF.B.h^Rg^J.B.l)&H_FLAG);               \
  R.AF.B.h=J.B.l       

#define M_SUB(Rg)      \
  J.W=R.AF.B.h-Rg;     \
  R.AF.B.l=            \
    ((R.AF.B.h^Rg)&(R.AF.B.h^J.B.l)&0x80? V_FLAG:0)| \
    N_FLAG|-J.B.h|ZSTable[J.B.l]|                    \
    ((R.AF.B.h^Rg^J.B.l)&H_FLAG);                    \
  R.AF.B.h=J.B.l

#define M_ADC(Rg)      \
  J.W=R.AF.B.h+Rg+(R.AF.B.l&C_FLAG); \
  R.AF.B.l=                          \
    (~(R.AF.B.h^Rg)&(Rg^J.B.l)&0x80? V_FLAG:0)| \
    J.B.h|ZSTable[J.B.l]|            \
    ((R.AF.B.h^Rg^J.B.l)&H_FLAG);    \
  R.AF.B.h=J.B.l

#define M_SBC(Rg)      \
  J.W=R.AF.B.h-Rg-(R.AF.B.l&C_FLAG); \
  R.AF.B.l=                          \
    ((R.AF.B.h^Rg)&(R.AF.B.h^J.B.l)&0x80? V_FLAG:0)| \
    N_FLAG|-J.B.h|ZSTable[J.B.l]|    \
    ((R.AF.B.h^Rg^J.B.l)&H_FLAG);    \
  R.AF.B.h=J.B.l

#define M_CP(Rg)       \
  J.W=R.AF.B.h-Rg;     \
  R.AF.B.l=            \
    ((R.AF.B.h^Rg)&(R.AF.B.h^J.B.l)&0x80? V_FLAG:0)| \
    N_FLAG|-J.B.h|ZSTable[J.B.l]|                    \
    ((R.AF.B.h^Rg^J.B.l)&H_FLAG)

#define M_AND(Rg) R.AF.B.h&=Rg;R.AF.B.l=H_FLAG|PZSTable[R.AF.B.h]
#define M_OR(Rg)  R.AF.B.h|=Rg;R.AF.B.l=PZSTable[R.AF.B.h]
#define M_XOR(Rg) R.AF.B.h^=Rg;R.AF.B.l=PZSTable[R.AF.B.h]
#define M_IN(Rg)  Rg=DoIn(R.BC.B.l);R.AF.B.l=PZSTable[Rg]|(R.AF.B.l&C_FLAG)

#define M_INC(Rg)       \
  Rg++;                 \
  R.AF.B.l=             \
    (R.AF.B.l&C_FLAG)|ZSTable[Rg]|           \
    (Rg==0x80? V_FLAG:0)|(Rg&0x0F? 0:H_FLAG)

#define M_DEC(Rg)       \
  Rg--;                 \
  R.AF.B.l=             \
    N_FLAG|(R.AF.B.l&C_FLAG)|ZSTable[Rg]|    \
    (Rg==0x7F? V_FLAG:0)|((Rg&0x0F)==0x0F? H_FLAG:0)

#define M_ADDW(Rg1,Rg2) \
  J.W=(R.Rg1.W+R.Rg2.W)&0xFFFF;                        \
  R.AF.B.l=                                            \
    (R.AF.B.l&~(H_FLAG|N_FLAG|C_FLAG))|                \
    ((R.Rg1.W^R.Rg2.W^J.W)&0x1000? H_FLAG:0)|          \
    (((long)R.Rg1.W+(long)R.Rg2.W)&0x10000? C_FLAG:0); \
  R.Rg1.W=J.W

#define M_ADCW(Rg)      \
  I=R.AF.B.l&C_FLAG;J.W=(R.HL.W+R.Rg.W+I)&0xFFFF;            \
  R.AF.B.l=                                                  \
    (((long)R.HL.W+(long)R.Rg.W+(long)I)&0x10000? C_FLAG:0)| \
    (~(R.HL.W^R.Rg.W)&(R.Rg.W^J.W)&0x8000? V_FLAG:0)|        \
    ((R.HL.W^R.Rg.W^J.W)&0x1000? H_FLAG:0)|                  \
    (J.W? 0:Z_FLAG)|(J.B.h&S_FLAG);                          \
  R.HL.W=J.W
   
#define M_SBCW(Rg)      \
  I=R.AF.B.l&C_FLAG;J.W=(R.HL.W-R.Rg.W-I)&0xFFFF;            \
  R.AF.B.l=                                                  \
    N_FLAG|                                                  \
    (((long)R.HL.W-(long)R.Rg.W-(long)I)&0x10000? C_FLAG:0)| \
    ((R.HL.W^R.Rg.W)&(R.HL.W^J.W)&0x8000? V_FLAG:0)|         \
    ((R.HL.W^R.Rg.W^J.W)&0x1000? H_FLAG:0)|                  \
    (J.W? 0:Z_FLAG)|(J.B.h&S_FLAG);                          \
  R.HL.W=J.W


enum Codes
{
  NOP,LD_BC_WORD,LD_xBC_A,INC_BC,INC_B,DEC_B,LD_B_BYTE,RLCA,
  EX_AF_AF,ADD_HL_BC,LD_A_xBC,DEC_BC,INC_C,DEC_C,LD_C_BYTE,RRCA,
  DJNZ,LD_DE_WORD,LD_xDE_A,INC_DE,INC_D,DEC_D,LD_D_BYTE,RLA,
  JR,ADD_HL_DE,LD_A_xDE,DEC_DE,INC_E,DEC_E,LD_E_BYTE,RRA,
  JR_NZ,LD_HL_WORD,LD_xWORD_HL,INC_HL,INC_H,DEC_H,LD_H_BYTE,DAA,
  JR_Z,ADD_HL_HL,LD_HL_xWORD,DEC_HL,INC_L,DEC_L,LD_L_BYTE,CPL,
  JR_NC,LD_SP_WORD,LD_xWORD_A,INC_SP,INC_xHL,DEC_xHL,LD_xHL_BYTE,SCF,
  JR_C,ADD_HL_SP,LD_A_xWORD,DEC_SP,INC_A,DEC_A,LD_A_BYTE,CCF,
  LD_B_B,LD_B_C,LD_B_D,LD_B_E,LD_B_H,LD_B_L,LD_B_xHL,LD_B_A,
  LD_C_B,LD_C_C,LD_C_D,LD_C_E,LD_C_H,LD_C_L,LD_C_xHL,LD_C_A,
  LD_D_B,LD_D_C,LD_D_D,LD_D_E,LD_D_H,LD_D_L,LD_D_xHL,LD_D_A,
  LD_E_B,LD_E_C,LD_E_D,LD_E_E,LD_E_H,LD_E_L,LD_E_xHL,LD_E_A,
  LD_H_B,LD_H_C,LD_H_D,LD_H_E,LD_H_H,LD_H_L,LD_H_xHL,LD_H_A,
  LD_L_B,LD_L_C,LD_L_D,LD_L_E,LD_L_H,LD_L_L,LD_L_xHL,LD_L_A,
  LD_xHL_B,LD_xHL_C,LD_xHL_D,LD_xHL_E,LD_xHL_H,LD_xHL_L,HALT,LD_xHL_A,
  LD_A_B,LD_A_C,LD_A_D,LD_A_E,LD_A_H,LD_A_L,LD_A_xHL,LD_A_A,
  ADD_B,ADD_C,ADD_D,ADD_E,ADD_H,ADD_L,ADD_xHL,ADD_A,
  ADC_B,ADC_C,ADC_D,ADC_E,ADC_H,ADC_L,ADC_xHL,ADC_A,
  SUB_B,SUB_C,SUB_D,SUB_E,SUB_H,SUB_L,SUB_xHL,SUB_A,
  SBC_B,SBC_C,SBC_D,SBC_E,SBC_H,SBC_L,SBC_xHL,SBC_A,
  AND_B,AND_C,AND_D,AND_E,AND_H,AND_L,AND_xHL,AND_A,
  XOR_B,XOR_C,XOR_D,XOR_E,XOR_H,XOR_L,XOR_xHL,XOR_A,
  OR_B,OR_C,OR_D,OR_E,OR_H,OR_L,OR_xHL,OR_A,
  CP_B,CP_C,CP_D,CP_E,CP_H,CP_L,CP_xHL,CP_A,
  RET_NZ,POP_BC,JP_NZ,JP,CALL_NZ,PUSH_BC,ADD_BYTE,RST00,
  RET_Z,RET,JP_Z,PFX_CB,CALL_Z,CALL,ADC_BYTE,RST08,
  RET_NC,POP_DE,JP_NC,OUTA,CALL_NC,PUSH_DE,SUB_BYTE,RST10,
  RET_C,EXX,JP_C,INA,CALL_C,PFX_DD,SBC_BYTE,RST18,
  RET_PO,POP_HL,JP_PO,EX_HL_xSP,CALL_PO,PUSH_HL,AND_BYTE,RST20,
  RET_PE,LD_PC_HL,JP_PE,EX_DE_HL,CALL_PE,PFX_ED,XOR_BYTE,RST28,
  RET_P,POP_AF,JP_P,DI,CALL_P,PUSH_AF,OR_BYTE,RST30,
  RET_M,LD_SP_HL,JP_M,EI,CALL_M,PFX_FD,CP_BYTE,RST38
};

enum CodesCB
{
  RLC_B,RLC_C,RLC_D,RLC_E,RLC_H,RLC_L,RLC_xHL,RLC_A,
  RRC_B,RRC_C,RRC_D,RRC_E,RRC_H,RRC_L,RRC_xHL,RRC_A,
  RL_B,RL_C,RL_D,RL_E,RL_H,RL_L,RL_xHL,RL_A,
  RR_B,RR_C,RR_D,RR_E,RR_H,RR_L,RR_xHL,RR_A,
  SLA_B,SLA_C,SLA_D,SLA_E,SLA_H,SLA_L,SLA_xHL,SLA_A,
  SRA_B,SRA_C,SRA_D,SRA_E,SRA_H,SRA_L,SRA_xHL,SRA_A,
  SLL_B,SLL_C,SLL_D,SLL_E,SLL_H,SLL_L,SLL_xHL,SLL_A,
  SRL_B,SRL_C,SRL_D,SRL_E,SRL_H,SRL_L,SRL_xHL,SRL_A,
  BIT0_B,BIT0_C,BIT0_D,BIT0_E,BIT0_H,BIT0_L,BIT0_xHL,BIT0_A,
  BIT1_B,BIT1_C,BIT1_D,BIT1_E,BIT1_H,BIT1_L,BIT1_xHL,BIT1_A,
  BIT2_B,BIT2_C,BIT2_D,BIT2_E,BIT2_H,BIT2_L,BIT2_xHL,BIT2_A,
  BIT3_B,BIT3_C,BIT3_D,BIT3_E,BIT3_H,BIT3_L,BIT3_xHL,BIT3_A,
  BIT4_B,BIT4_C,BIT4_D,BIT4_E,BIT4_H,BIT4_L,BIT4_xHL,BIT4_A,
  BIT5_B,BIT5_C,BIT5_D,BIT5_E,BIT5_H,BIT5_L,BIT5_xHL,BIT5_A,
  BIT6_B,BIT6_C,BIT6_D,BIT6_E,BIT6_H,BIT6_L,BIT6_xHL,BIT6_A,
  BIT7_B,BIT7_C,BIT7_D,BIT7_E,BIT7_H,BIT7_L,BIT7_xHL,BIT7_A,
  RES0_B,RES0_C,RES0_D,RES0_E,RES0_H,RES0_L,RES0_xHL,RES0_A,
  RES1_B,RES1_C,RES1_D,RES1_E,RES1_H,RES1_L,RES1_xHL,RES1_A,
  RES2_B,RES2_C,RES2_D,RES2_E,RES2_H,RES2_L,RES2_xHL,RES2_A,
  RES3_B,RES3_C,RES3_D,RES3_E,RES3_H,RES3_L,RES3_xHL,RES3_A,
  RES4_B,RES4_C,RES4_D,RES4_E,RES4_H,RES4_L,RES4_xHL,RES4_A,
  RES5_B,RES5_C,RES5_D,RES5_E,RES5_H,RES5_L,RES5_xHL,RES5_A,
  RES6_B,RES6_C,RES6_D,RES6_E,RES6_H,RES6_L,RES6_xHL,RES6_A,
  RES7_B,RES7_C,RES7_D,RES7_E,RES7_H,RES7_L,RES7_xHL,RES7_A,  
  SET0_B,SET0_C,SET0_D,SET0_E,SET0_H,SET0_L,SET0_xHL,SET0_A,
  SET1_B,SET1_C,SET1_D,SET1_E,SET1_H,SET1_L,SET1_xHL,SET1_A,
  SET2_B,SET2_C,SET2_D,SET2_E,SET2_H,SET2_L,SET2_xHL,SET2_A,
  SET3_B,SET3_C,SET3_D,SET3_E,SET3_H,SET3_L,SET3_xHL,SET3_A,
  SET4_B,SET4_C,SET4_D,SET4_E,SET4_H,SET4_L,SET4_xHL,SET4_A,
  SET5_B,SET5_C,SET5_D,SET5_E,SET5_H,SET5_L,SET5_xHL,SET5_A,
  SET6_B,SET6_C,SET6_D,SET6_E,SET6_H,SET6_L,SET6_xHL,SET6_A,
  SET7_B,SET7_C,SET7_D,SET7_E,SET7_H,SET7_L,SET7_xHL,SET7_A
};
  
enum CodesED
{
  DB_00,DB_01,DB_02,DB_03,DB_04,DB_05,DB_06,DB_07,
  DB_08,DB_09,DB_0A,DB_0B,DB_0C,DB_0D,DB_0E,DB_0F,
  DB_10,DB_11,DB_12,DB_13,DB_14,DB_15,DB_16,DB_17,
  DB_18,DB_19,DB_1A,DB_1B,DB_1C,DB_1D,DB_1E,DB_1F,
  DB_20,DB_21,DB_22,DB_23,DB_24,DB_25,DB_26,DB_27,
  DB_28,DB_29,DB_2A,DB_2B,DB_2C,DB_2D,DB_2E,DB_2F,
  DB_30,DB_31,DB_32,DB_33,DB_34,DB_35,DB_36,DB_37,
  DB_38,DB_39,DB_3A,DB_3B,DB_3C,DB_3D,DB_3E,DB_3F,
  IN_B_xC,OUT_xC_B,SBC_HL_BC,LD_xWORDe_BC,NEG,RETN,IM_0,LD_I_A,
  IN_C_xC,OUT_xC_C,ADC_HL_BC,LD_BC_xWORDe,DB_4C,RETI,DB_,LD_R_A,
  IN_D_xC,OUT_xC_D,SBC_HL_DE,LD_xWORDe_DE,DB_54,DB_55,IM_1,LD_A_I,
  IN_E_xC,OUT_xC_E,ADC_HL_DE,LD_DE_xWORDe,DB_5C,DB_5D,IM_2,LD_A_R,
  IN_H_xC,OUT_xC_H,SBC_HL_HL,LD_xWORDe_HL,DB_64,DB_65,DB_66,RRD,
  IN_L_xC,OUT_xC_L,ADC_HL_HL,LD_HL_xWORDe,DB_6C,DB_6D,DB_6E,RLD,
  IN_F_xC,DB_71,SBC_HL_SP,LD_xWORDe_SP,DB_74,DB_75,DB_76,DB_77,
  IN_A_xC,OUT_xC_A,ADC_HL_SP,LD_SP_xWORDe,DB_7C,DB_7D,DB_7E,DB_7F,
  DB_80,DB_81,DB_82,DB_83,DB_84,DB_85,DB_86,DB_87,
  DB_88,DB_89,DB_8A,DB_8B,DB_8C,DB_8D,DB_8E,DB_8F,
  DB_90,DB_91,DB_92,DB_93,DB_94,DB_95,DB_96,DB_97,
  DB_98,DB_99,DB_9A,DB_9B,DB_9C,DB_9D,DB_9E,DB_9F,
  LDI,CPI,INI,OUTI,DB_A4,DB_A5,DB_A6,DB_A7,
  LDD,CPD,IND,OUTD,DB_AC,DB_AD,DB_AE,DB_AF,
  LDIR,CPIR,INIR,OTIR,DB_B4,DB_B5,DB_B6,DB_B7,
  LDDR,CPDR,INDR,OTDR,DB_BC,DB_BD,DB_BE,DB_BF,
  DB_C0,DB_C1,DB_C2,DB_C3,DB_C4,DB_C5,DB_C6,DB_C7,
  DB_C8,DB_C9,DB_CA,DB_CB,DB_CC,DB_CD,DB_CE,DB_CF,
  DB_D0,DB_D1,DB_D2,DB_D3,DB_D4,DB_D5,DB_D6,DB_D7,
  DB_D8,DB_D9,DB_DA,DB_DB,DB_DC,DB_DD,DB_DE,DB_DF,
  DB_E0,DB_E1,DB_E2,DB_E3,DB_E4,DB_E5,DB_E6,DB_E7,
  DB_E8,DB_E9,DB_EA,DB_EB,DB_EC,DB_ED,DB_EE,DB_EF,
  DB_F0,DB_F1,DB_F2,DB_F3,DB_F4,DB_F5,DB_F6,DB_F7,
  DB_F8,DB_F9,DB_FA,DB_FB,DB_FC,DB_FD,DB_FE,DB_FF
};


static void CodesCB(void)
{
  register byte I;

  ClockCount-=cycles_cb[I=M_RDMEM(R.PC.W++)];
  switch(I)
  {
#include "CodesCB.h"
    default:
      if(TrapBadOps)
        printf
        (   
          "Unrecognized instruction: CB %X at PC=%hX\n",
          M_RDMEM(R.PC.W-1),R.PC.W-2
        );
  }
}

static void CodesDDCB(void)
{
  register pair J;
  register byte I;

#define XX IX    
  J.W=R.XX.W+(offset)M_RDMEM(R.PC.W++);

  ClockCount-=cycles_xx_cb[I=M_RDMEM(R.PC.W++)];
  switch(I)
  {
#include "CodesXCB.h"
    default:
      if(TrapBadOps)
        printf
        (
          "Unrecognized instruction: DD CB %X %X at PC=%hX\n",
          M_RDMEM(R.PC.W-2),M_RDMEM(R.PC.W-1),R.PC.W-4
        );
  }
#undef XX
}

static void CodesFDCB(void)
{
  register pair J;
  register byte I;

#define XX IY
  J.W=R.XX.W+(offset)M_RDMEM(R.PC.W++);

  ClockCount-=cycles_xx_cb[I=M_RDMEM(R.PC.W++)];
  switch(I)
  {
#include "CodesXCB.h"
    default:
      if(TrapBadOps)
        printf
        (
          "Unrecognized instruction: FD CB %X %X at PC=%hX\n",
          M_RDMEM(R.PC.W-2),M_RDMEM(R.PC.W-1),R.PC.W-4
        );
  }
#undef XX
}

static void CodesED(void)
{
  register byte I;
  register pair J;

  /*ClockCount-=cycles_xx[I=M_RDMEM(R.PC.W++)];*/
  ClockCount-=cycles_ed[I=M_RDMEM(R.PC.W++)];
  switch(I)
  {
#include "CodesED.h"
    case PFX_ED:
      R.PC.W--;break;
    default:
      if(TrapBadOps)
        printf
        (
          "Unrecognized instruction: ED %X at PC=%hX\n",
          M_RDMEM(R.PC.W-1),R.PC.W-2
        );
  }
}

static void CodesDD(void)
{
  register byte I;
  register pair J;

#define XX IX
  ClockCount-=cycles_xx[I=M_RDMEM(R.PC.W++)];
  switch(I)
  {
#include "CodesXX.h"
    case PFX_FD:
    case PFX_DD:
      R.PC.W--;break;
    case PFX_CB:
      CodesDDCB();break;
    case HALT:
#ifdef INTERRUPTS
      if(R.IFF&0x01) { R.PC.W--;R.IFF|=0x80; }
#else
      printf("CPU HALTed and stuck at PC=%hX\n",R.PC.W-=2);
      CPURunning=0;
#endif
      break;
    default:
      if(TrapBadOps)
        printf
        (
          "Unrecognized instruction: DD %X at PC=%hX\n",
          M_RDMEM(R.PC.W-1),R.PC.W-2
        );
  }
#undef XX
}

static void CodesFD(void)
{
  register byte I;
  register pair J;

#define XX IY
  ClockCount-=cycles_xx[I=M_RDMEM(R.PC.W++)];
  switch(I)
  {
#include "CodesXX.h"
    case PFX_FD:
    case PFX_DD:
      R.PC.W--;break;
    case PFX_CB:
      CodesFDCB();break;
    case HALT:
#ifdef INTERRUPTS
      if(R.IFF&0x01) { R.PC.W--;R.IFF|=0x80; }
#else
      printf("CPU HALTed and stuck at PC=%hX\n",R.PC.W-=2);
      CPURunning=0;
#endif
      break;
    default:
        printf
        (
          "Unrecognized instruction: FD %X at PC=%hX\n",
          M_RDMEM(R.PC.W-1),R.PC.W-2
        );
  }
#undef XX
}

/*** Reset Z80 registers: *********************************/
/*** This function can be used to reset the register    ***/
/*** file before starting execution with Z80(). It sets ***/
/*** the registers to their initial values.             ***/
/**********************************************************/
void ResetZ80(void)
{
  printf("ResetZ80 \n");

  R.PC.W=0x0000;R.SP.W=0xF000;
  R.AF.W=R.BC.W=R.DE.W=R.HL.W=0x0000;
  R.AF1.W=R.BC1.W=R.DE1.W=R.HL1.W=0x0000;
  R.IX.W=R.IY.W=0x0000;
  R.I=0x00;R.IFF=0x00;

  ClockCount = 0;
  save_intr=0xffff;

  CPURunning=1;
  TClock = 0;

  TintClockRemain = TimerInt_Clock;		// タイマ割り込み監視
  CmtClockRemain = CmtInt_Clock;			// CMT割り込み監視
}

//クロック周波数を得る
long GetClock(void)
{
  return z80_clock;
}

//
// Z80が動作する事ができる水平ライン数
//
void SetValidLine(long Line)
{
  Valid_Line = 262 - Line;
}

//
// クロック周波数を設定
// 合わせてタイマ・CMT割り込み周期も算出する
//
void SetClock(long clock)
{
  z80_clock = clock;

  // 4MHzの場合、2083に近い値になる?
  BaseTimerClock = (long)( ((float)(clock/(60*262)) * (float)(Valid_Line)) / 8.533332992f);
  TimerInt_Clock = BaseTimerClock;

//	CmtInt_Clock = ((clock/(60*262)) * Valid_Line) / 2;
//	CmtInt_Clock = CmtInt_Clock * 2;		// ← 暫定的処理！

  // 常に固定
  CmtInt_Clock = 33333;
}

// タイマー割り込みのカウント
// 推測:初期値は3で、この時2msec。たぶん、4count = 2msec。
void SetTimerIntClock(long pow)
{
  TimerInt_Clock = BaseTimerClock * ((pow + 1) / 4);
}

// これまでに実行したマシンカウントを取得する
long GetTClock(void)
{
  long retValue = TClock;
  TClock = 0;
  return retValue;
}

/*extern int TimerSWFlag;*/
int TimerSWFlag = 1;
int WaitFlag = 1;
extern int portF7;
void unixIdle();

  static int HCount = 0;
  int	NowClock;
  long StartCount;
  int hline;

/*** Interpret Z80 code: **********************************/
/*** Registers have initial values from Regs. PC value  ***/
/*** at which emulation stopped is returned by this     ***/
/*** function.                                          ***/
/**********************************************************/
word Z80(void)
{
 // printf("Z80() proc.. \n");

 int cnt =0;
  register byte I, j;
  register pair J;
/*
  static int HCount = 0;
  int	NowClock;
  long StartCount;
  int hline;
*/
  CPURunning=1;
#ifndef __EMSCRIPTEN__
  for(;;)
#endif
  {   
    //  SDL_Log("ClockCount %d Valid_Line %d\n",ClockCount , Valid_Line); 
    // 一画面分の水平走査線 - Z80がBUSREQで停止している走査線数 = Z80稼動走査線数
    ClockCount += z80_clock/(60*262);	// 1水平ライン分のクロック数
    for (hline = 0; hline < Valid_Line; /*hline++*/) {
		// 1水平ライン分の処理を開始する
			while(ClockCount > 0) {
#ifdef DEBUG
				/* Turn tracing on when reached trap address */
				if(R.PC.W==Trap) Trace=1;
				/* Call single-step debugger, exit if requested */
				if(Trace) if(!Debug(&R)) return(R.PC.W);
#endif
				StartCount = ClockCount;
				switch((j=M_RDMEM(R.PC.W++)))
					{
					#include "Codes.h"
					case PFX_CB: CodesCB();break;
					case PFX_ED: CodesED();break;
					case PFX_FD: CodesFD();break;
					case PFX_DD: CodesDD();break;
					case HALT: 
								ClockCount=0;
#ifdef INTERRUPTS
							if(R.IFF&0x01) { R.PC.W--;R.IFF|=0x80; }
#else
							printf("CPU HALTed and stuck at PC=%hX\n",--R.PC.W);
							CPURunning=0;
#endif   
							R.IFF|=0x80;
								break;
					default:
							if(TrapBadOps)
								printf
							(
							"Unrecognized instruction: %X at PC=%hX\n",
							M_RDMEM(R.PC.W-1),R.PC.W-1
							);
					}

				ClockCount-=cycles_main[j]+1;
				NowClock = StartCount - ClockCount;
				/* 実行したクロック数を保存する */
				TClock += NowClock;
				/* タイマー割り込みクロック数の計算 */
				TintClockRemain-= NowClock;
				/* CMT割り込みクロック数の計算 */
				CmtClockRemain-= NowClock;

				/* タイマー割り込みの発生？ */
				if (TintClockRemain <= 0) {
					/*TintClockRemain += TimerInt_Clock;*/
					TintClockRemain = TimerInt_Clock;
					if(TimerSW && TimerSWFlag & TimerSW_F3) {
							IFlag=1;
							TimerIntFlag = INTFLAG_REQ;
					}
				}
			
				/* CMT割り込みの発生許可？ */
				if (CmtClockRemain <= 0) {
					/* 割り込み許可状態？ */
					if (R.IFF&0x01) {
							IFlag = 1;
							CmtClockRemain = CmtInt_Clock;
							enableCmtInt();
					}
				}

				if ((TimerIntFlag == INTFLAG_REQ) ||
					(CmtIntFlag == INTFLAG_REQ) ||
					(KeyIntFlag == INTFLAG_REQ))
					break;
				} /* while(ClockCount > 0) { */

			/* １画面分の有効スキャンラインにかかる処理時間分を実行したら
			画面の更新処理 */
			/* 262は表示する画面のスキャンライン数 */
			if (ClockCount <= 0) {
				hline++;
				ClockCount += z80_clock/(60*262);
				// キーボード状態をチェックする
				Keyboard();
				if (++HCount == 262) {
					HCount = 0;
					// キーボード状態をチェックする
					/*Keyboard();*/
					/* 割り込みを発生させる
						（他の割り込み処理を帰線期間で検出させるため） */
					IFlag=1;
				}
			}

			if(IFlag)
				{
				if(!CPURunning) return(R.PC.W); /*break;*/ /* add */
				//if(!CPURunning) return; /*break;*/ /* add */
				IFlag=0;J.W=Interrupt();
				
				if(((J.W!=0xFFFF)&&(R.IFF&0x01))||(J.W==0x0066)){
					/* which interrupt? */
					if (J.W == INTADDR_TIMER) TimerIntFlag = INTFLAG_NONE;
					else if (J.W == INTADDR_CMTREAD) CmtIntFlag = INTFLAG_EXEC;
					else if (J.W == INTADDR_STRIG) StrigIntFlag = INTFLAG_EXEC;
					else if ((J.W == INTADDR_KEY1) || (J.W == INTADDR_KEY2))
					KeyIntFlag = INTFLAG_EXEC;

					/* Experimental V Shouldn't disable all interrupts? */
					R.IFF=(R.IFF&0xBE)|((R.IFF&0x01)<<6);
					if(R.IFF&0x80)  { R.PC.W++;R.IFF&=0x7F; }
					M_PUSH(PC);

					if(J.W==0x0066) R.PC.W=0x0066; /* NMI */
					else
					if(R.IFF&0x04) /* IM 2 */
						{ 
						J.W&=0xFE;J.B.h=R.I;
						R.PC.B.l=M_RDMEM(J.W++);
						R.PC.B.h=M_RDMEM(J.W);
						}
					else
						if(R.IFF&0x02) 
							R.PC.W=0x0038; /* IM 1 */
						else 
							R.PC.W=J.W; /* IM 0 */
				}
			}
		} /* for (hline = 0; hline < Valid_Line; hline++) */

		/* 画面を更新する */
		//printf("UpdateScreen \n");
		UpdateScreen();

		/* idle */
	if (WaitFlag) unixIdle();
	} /* for(;;) */
//  return(R.PC.W);
}



/****************************************************************/
/*** Refresh screen, check keyboard and sprites. Call this    ***/
/*** function on each interrupt.                              ***/
/****************************************************************/
word Interrupt(void)
{
  /* interrupt priority (PC-6601SR) */
  /* 1.Timer     , 2.subCPU    , 3.Voice     , 4.VRTC      */
  /* 5.RS-232C   , 6.Joy Stick , 7.EXT INT   , 8.Printer   */

  if (TimerIntFlag == INTFLAG_REQ) return(INTADDR_TIMER); /* timer interrupt */
  else if (CasMode && (p6key == 0xFA) && (keyGFlag == 1))
    return(INTADDR_CMTSTOP); /* Press STOP while CMT Load or Save */
  else if ((CasMode==CAS_LOADING) && (CmtIntFlag == INTFLAG_REQ)) {
    /* CMT Loading */
    CmtIntFlag = INTFLAG_NONE;
    if(!feof(CasStream)) { /* if not EOF then Interrupt to Load 1 byte */
      CasMode=CAS_LOADBYTE;
      return(INTADDR_CMTREAD);
    } else { /* if EOF then Error */
      printf("tape file reached EOF\n");
      CasMode=CAS_NONE;
      return(INTADDR_CMTSTOP); /* Break */
    }
  } else if ((StrigIntFlag == INTFLAG_REQ) && IntSW_F3) /* if command 6 */
    return(INTADDR_STRIG);
  else if ((KeyIntFlag == INTFLAG_REQ) && IntSW_F3) /* if any key pressed */
    if (keyGFlag == 0) return(INTADDR_KEY1); /* normal key */
    else return(INTADDR_KEY2); /* special key (graphic key, etc.) */
  else /* none */
    return(INT_NONE);
}
