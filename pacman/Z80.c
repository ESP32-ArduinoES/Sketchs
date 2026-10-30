/** Z80: portable Z80 emulator *******************************/
/**                                                         **/
/**                           Z80.c                         **/
/**                                                         **/
/** This file contains implementation for Z80 CPU. Don't    **/
/** forget to provide RdZ80(), WrZ80(), InZ80(), OutZ80(),  **/
/** LoopZ80(), and PatchZ80() functions to accomodate the   **/
/** emulated machine's architecture.                        **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-2007                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

/*
  ============================================================================
  NOTAS GENERALES (en español)
  ============================================================================
  Este archivo implementa el emulador de la CPU Z80. Está diseñado para ser
  "portable": no sabe nada del hardware concreto. Para funcionar, el proyecto
  que lo usa debe proporcionar estas funciones:
    - RdZ80(addr)      : leer un byte de memoria (para datos).
    - WrZ80(addr, val) : escribir un byte en memoria.
    - InZ80(port)      : leer de un puerto de E/S.
    - OutZ80(port,val) : escribir a un puerto de E/S.
    - LoopZ80(R)       : hook periódico (para interrupciones).
    - PatchZ80(R)      : hook por instrucción (aquí no se usa).

  Galagino usa StepZ80() para ejecutar una instrucción por llamada desde el
  bucle de emulación. Eso permite controlar el timing con precisión.

  En Galagino:
    - RdZ80/WrZ80/InZ80/OutZ80 los proporciona emulation.c.
    - OpZ80_INL está definida en emulation.h (lectura de ROM directa).
  ============================================================================
*/

#include "Z80.h"        // Estructura Z80, tipos byte/z80_word/pair.
#include "Tables.h"     // Tablas de ciclos y de flags (Cycles[], ZSTable[], etc.).
#include <stdio.h>      // printf para el debug de opcodes no reconocidos.

/** INLINE ***************************************************/
/** C99 standard has "inline", but older compilers used     **/
/** __inline for the same purpose.                          **/
/*************************************************************/
#ifdef __C99__
#define INLINE static inline
#else
#define INLINE static __inline
#endif

/*
  Macro INLINE: en C99 se usa "static inline"; en compiladores antiguos,
  "__inline". En ESP32 con Arduino siempre será C99 o posterior, así que
  se resuelve a "static inline".
  "static" evita múltiples definiciones; "inline" sugiere al compilador
  expandir la función en el sitio de la llamada.
*/

/** System-Dependent Stuff ***********************************/
/** This is system-dependent code put here to speed things  **/
/** up. It has to stay inlined to be fast.                  **/
/*************************************************************/
#ifdef COLEM
#define RdZ80 RDZ80
extern byte *ROMPage[];
INLINE byte RdZ80(z80_word A) { return(ROMPage[A>>13][A&0x1FFF]); }
#endif

#ifdef SPECCY
#define RdZ80 RDZ80
#define WrZ80 WRZ80
extern byte *Page[],*ROM;
INLINE byte RdZ80(z80_word A)        { return(Page[A>>13][A&0x1FFF]); }
INLINE void WrZ80(z80_word A,byte V) { if(Page[A>>13]<ROM) Page[A>>13][A&0x1FFF]=V; }
#endif

#ifdef MG
#define RdZ80 RDZ80
extern byte *Page[];
INLINE byte RdZ80(z80_word A) { return(Page[A>>13][A&0x1FFF]); }
#endif

#ifdef FMSX
#define FAST_RDOP
extern byte *RAM[];
INLINE byte OpZ80(z80_word A) { return(RAM[A>>13][A&0x1FFF]); }
#endif

/*
  Estos bloques son específicos de otras máquinas (ColecoVision, ZX Spectrum,
  MSX, FMSX...). En Galagino NO se definen, así que no se compilan.
  Son "atajos" para acelerar la lectura de memoria usando paginación por
  bloques de 8 KB (A>>13 da el bloque, A&0x1FFF el offset dentro del bloque).
*/

/** FAST_RDOP ************************************************/
/** With this #define not present, RdZ80() should perform   **/
/** the functions of OpZ80().                               **/
/*************************************************************/
#ifndef FAST_RDOP
#define OpZ80(A) RdZ80(A)
#endif

/*
  FAST_RDOP define si hay una función especializada para leer OPCODES
  (instrucciones) distinta de la de leer DATOS. En Galagino no se define
  FAST_RDOP, así que OpZ80 se resuelve a RdZ80. Sin embargo, Galagino
  SÍ tiene su propio OpZ80_INL definido en emulation.h, que se usa en
  StepZ80() en lugar de esta macro.
*/

/* ==========================================================================
   MACROS DE FLAGS
   El registro F del Z80 tiene bits de estado que muchas instrucciones
   actualizan. Estas macros permiten ponerlos/quitaros de forma compacta.
   ========================================================================== */

#define S(Fl)        R->AF.B.l|=Fl       // Set: activa los bits Fl en F.
#define R(Fl)        R->AF.B.l&=~(Fl)    // Reset: apaga los bits Fl en F.
#define FLAGS(Rg,Fl) R->AF.B.l=Fl|ZSTable[Rg]  // F = Fl + flags Z/S de Rg.

/*
  ZSTable[Rg] es una tabla que devuelve los flags Z (zero) y S (sign) según
  el valor de Rg. Así no hay que calcularlos con comparaciones.
*/

/* ==========================================================================
   MACROS DE ROTACIÓN Y DESPLAZAMIENTO (grupo CB)
   Estas son las instrucciones RLC, RRC, RL, RR, SLA, SRA, SLL, SRL del Z80.
   ========================================================================== */

#define M_RLC(Rg)      \
  R->AF.B.l=Rg>>7;Rg=(Rg<<1)|R->AF.B.l;R->AF.B.l|=PZSTable[Rg]

/*
  RLC: rotación circular a la izquierda.
  - El bit 7 sale y entra por el bit 0.
  - Ese bit 7 se guarda en CF.
  - Luego se añaden los flags P/Z/S.
*/

#define M_RRC(Rg)      \
  R->AF.B.l=Rg&0x01;Rg=(Rg>>1)|(R->AF.B.l<<7);R->AF.B.l|=PZSTable[Rg]

/*
  RRC: rotación circular a la derecha.
  - El bit 0 sale y entra por el bit 7.
  - Ese bit 0 se guarda en CF.
*/

#define M_RL(Rg)       \
  if(Rg&0x80)          \
  {                    \
    Rg=(Rg<<1)|(R->AF.B.l&C_FLAG); \
    R->AF.B.l=PZSTable[Rg]|C_FLAG; \
  }                    \
  else                 \
  {                    \
    Rg=(Rg<<1)|(R->AF.B.l&C_FLAG); \
    R->AF.B.l=PZSTable[Rg];        \
  }

/*
  RL: rotación a la izquierda a través del carry.
  - El bit 7 sale a CF, el CF anterior entra por el bit 0.
  - Si el bit 7 era 1, CF=1; si era 0, CF=0.
*/

#define M_RR(Rg)       \
  if(Rg&0x01)          \
  {                    \
    Rg=(Rg>>1)|(R->AF.B.l<<7);     \
    R->AF.B.l=PZSTable[Rg]|C_FLAG; \
  }                    \
  else                 \
  {                    \
    Rg=(Rg>>1)|(R->AF.B.l<<7);     \
    R->AF.B.l=PZSTable[Rg];        \
  }

/*
  RR: rotación a la derecha a través del carry.
  - El bit 0 sale a CF, el CF anterior entra por el bit 7.
*/

#define M_SLA(Rg)      \
  R->AF.B.l=Rg>>7;Rg<<=1;R->AF.B.l|=PZSTable[Rg]

// SLA: shift left arithmetic. Desplaza a la izquierda, entra 0 por el bit 0.

#define M_SRA(Rg)      \
  R->AF.B.l=Rg&C_FLAG;Rg=(Rg>>1)|(Rg&0x80);R->AF.B.l|=PZSTable[Rg]

// SRA: shift right arithmetic. Desplaza a la derecha, el bit 7 se conserva.

#define M_SLL(Rg)      \
  R->AF.B.l=Rg>>7;Rg=(Rg<<1)|0x01;R->AF.B.l|=PZSTable[Rg]

// SLL: shift left logical. Es un "undocumented" del Z80. Entra 1 por el bit 0.

#define M_SRL(Rg)      \
  R->AF.B.l=Rg&0x01;Rg>>=1;R->AF.B.l|=PZSTable[Rg]

// SRL: shift right logical. Desplaza a la derecha, entra 0 por el bit 7.

/* ==========================================================================
   MACROS DE BITS (grupo CB, instrucciones BIT/SET/RES)
   ========================================================================== */

#define M_BIT(Bit,Rg)  \
  R->AF.B.l=(R->AF.B.l&C_FLAG)|H_FLAG|PZSTable[Rg&(1<<Bit)]

/*
  BIT: comprueba el bit "Bit" de Rg.
  - CF se conserva.
  - H se pone siempre a 1 (comportamiento real del Z80).
  - Z y S se calculan según el valor del bit.
  - No modifica Rg.
*/

#define M_SET(Bit,Rg) Rg|=1<<Bit    // SET: pone a 1 el bit "Bit" de Rg.
#define M_RES(Bit,Rg) Rg&=~(1<<Bit) // RES: pone a 0 el bit "Bit" de Rg.

/* ==========================================================================
   MACROS DE PILA (PUSH/POP)
   ========================================================================== */

#define M_POP(Rg)      \
  R->Rg.B.l=OpZ80(R->SP.W++);R->Rg.B.h=OpZ80(R->SP.W++)

// POP: saca dos bytes de la pila (primero el bajo, luego el alto).

#define M_PUSH(Rg)     \
  WrZ80(--R->SP.W,R->Rg.B.h);WrZ80(--R->SP.W,R->Rg.B.l)

// PUSH: mete dos bytes en la pila (primero el alto, luego el bajo).

/* ==========================================================================
   MACROS DE CONTROL DE FLUJO (CALL/JP/JR/RET/RST)
   ========================================================================== */

#define M_CALL         \
  J.B.l=OpZ80(R->PC.W++);J.B.h=OpZ80(R->PC.W++);         \
  WrZ80(--R->SP.W,R->PC.B.h);WrZ80(--R->SP.W,R->PC.B.l); \
  R->PC.W=J.W; \
  JumpZ80(J.W)

/*
  CALL: llamada a subrutina.
  1. Lee la dirección destino de los dos bytes siguientes.
  2. Apila la dirección de retorno (PC actual).
  3. Salta al destino.
  JumpZ80() es una macro de Z80.h que probablemente actualiza el PC.
*/

#define M_JP  J.B.l=OpZ80(R->PC.W++);J.B.h=OpZ80(R->PC.W);R->PC.W=J.W;JumpZ80(J.W)
// JP: salto incondicional a una dirección absoluta de 16 bits.

#define M_JR  R->PC.W+=(offset)OpZ80(R->PC.W)+1;JumpZ80(R->PC.W)
// JR: salto relativo (offset de 8 bits con signo) desde el PC actual.

#define M_RET R->PC.B.l=OpZ80(R->SP.W++);R->PC.B.h=OpZ80(R->SP.W++);JumpZ80(R->PC.W)
// RET: retorno de subrutina (saca PC de la pila).

#define M_RST(Ad)      \
  WrZ80(--R->SP.W,R->PC.B.h);WrZ80(--R->SP.W,R->PC.B.l);R->PC.W=Ad;JumpZ80(Ad)
// RST: reset a una dirección fija (0x00, 0x08, 0x10, ..., 0x38).

#define M_LDWORD(Rg)   \
  R->Rg.B.l=OpZ80(R->PC.W++);R->Rg.B.h=OpZ80(R->PC.W++)
// LD Rg, (word): carga un valor de 16 bits en un registro.

/* ==========================================================================
   MACROS ARITMÉTICAS (ADD/SUB/ADC/SBC/CP)
   Aquí está la "magia" del cálculo de flags. Es denso, pero eficiente.
   ========================================================================== */

#define M_ADD(Rg)      \
  J.W=R->AF.B.h+Rg;    \
  R->AF.B.l=           \
    (~(R->AF.B.h^Rg)&(Rg^J.B.l)&0x80? V_FLAG:0)| \
    J.B.h|ZSTable[J.B.l]|                        \
    ((R->AF.B.h^Rg^J.B.l)&H_FLAG);               \
  R->AF.B.h=J.B.l       

/*
  ADD A, Rg.
  - J.W = A + Rg (16 bits para capturar el carry).
  - Overflow (V): se calcula con la fórmula (~(A^Rg) & (Rg^J) & 0x80).
  - Carry (C): está en J.B.h (el byte alto de la suma).
  - Z y S: con ZSTable.
  - Half-carry (H): con XOR de bits 4.
*/

#define M_SUB(Rg)      \
  J.W=R->AF.B.h-Rg;    \
  R->AF.B.l=           \
    ((R->AF.B.h^Rg)&(R->AF.B.h^J.B.l)&0x80? V_FLAG:0)| \
    N_FLAG|-J.B.h|ZSTable[J.B.l]|                      \
    ((R->AF.B.h^Rg^J.B.l)&H_FLAG);                     \
  R->AF.B.h=J.B.l

// SUB A, Rg. Similar a ADD, pero con N_FLAG activado.

#define M_ADC(Rg)      \
  J.W=R->AF.B.h+Rg+(R->AF.B.l&C_FLAG); \
  R->AF.B.l=                           \
    (~(R->AF.B.h^Rg)&(Rg^J.B.l)&0x80? V_FLAG:0)| \
    J.B.h|ZSTable[J.B.l]|              \
    ((R->AF.B.h^Rg^J.B.l)&H_FLAG);     \
  R->AF.B.h=J.B.l

// ADC A, Rg. Como ADD, pero suma también el carry anterior.

#define M_SBC(Rg)      \
  J.W=R->AF.B.h-Rg-(R->AF.B.l&C_FLAG); \
  R->AF.B.l=                           \
    ((R->AF.B.h^Rg)&(R->AF.B.h^J.B.l)&0x80? V_FLAG:0)| \
    N_FLAG|-J.B.h|ZSTable[J.B.l]|      \
    ((R->AF.B.h^Rg^J.B.l)&H_FLAG);     \
  R->AF.B.h=J.B.l

// SBC A, Rg. Como SUB, pero resta también el carry anterior.

#define M_CP(Rg)       \
  J.W=R->AF.B.h-Rg;    \
  R->AF.B.l=           \
    ((R->AF.B.h^Rg)&(R->AF.B.h^J.B.l)&0x80? V_FLAG:0)| \
    N_FLAG|-J.B.h|ZSTable[J.B.l]|                      \
    ((R->AF.B.h^Rg^J.B.l)&H_FLAG)

// CP Rg. Como SUB, pero NO guarda el resultado en A (solo actualiza flags).

/* ==========================================================================
   MACROS LÓGICAS (AND/OR/XOR)
   ========================================================================== */

#define M_AND(Rg) R->AF.B.h&=Rg;R->AF.B.l=H_FLAG|PZSTable[R->AF.B.h]
// AND: A = A & Rg. H se pone a 1.

#define M_OR(Rg)  R->AF.B.h|=Rg;R->AF.B.l=PZSTable[R->AF.B.h]
// OR: A = A | Rg.

#define M_XOR(Rg) R->AF.B.h^=Rg;R->AF.B.l=PZSTable[R->AF.B.h]
// XOR: A = A ^ Rg.

/* ==========================================================================
   MACRO DE ENTRADA (IN)
   ========================================================================== */

#define M_IN(Rg)        \
  Rg=InZ80(R->BC.W);  \
  R->AF.B.l=PZSTable[Rg]|(R->AF.B.l&C_FLAG)

// IN Rg, (C): lee un byte del puerto BC y lo guarda en Rg. CF se conserva.

/* ==========================================================================
   MACROS DE INCREMENTO/DECREMENTO
   ========================================================================== */

#define M_INC(Rg)       \
  Rg++;                 \
  R->AF.B.l=            \
    (R->AF.B.l&C_FLAG)|ZSTable[Rg]|           \
    (Rg==0x80? V_FLAG:0)|(Rg&0x0F? 0:H_FLAG)

// INC Rg. CF se conserva. V se activa si pasa de 0x7F a 0x80.

#define M_DEC(Rg)       \
  Rg--;                 \
  R->AF.B.l=            \
    N_FLAG|(R->AF.B.l&C_FLAG)|ZSTable[Rg]| \
    (Rg==0x7F? V_FLAG:0)|((Rg&0x0F)==0x0F? H_FLAG:0)

// DEC Rg. CF se conserva. V se activa si pasa de 0x80 a 0x7F.

/* ==========================================================================
   MACROS ARITMÉTICAS DE 16 BITS (ADD/ADC/SBC con HL)
   ========================================================================== */

#define M_ADDW(Rg1,Rg2) \
  J.W=(R->Rg1.W+R->Rg2.W)&0xFFFF;                        \
  R->AF.B.l=                                             \
    (R->AF.B.l&~(H_FLAG|N_FLAG|C_FLAG))|                 \
    ((R->Rg1.W^R->Rg2.W^J.W)&0x1000? H_FLAG:0)|          \
    (((long)R->Rg1.W+(long)R->Rg2.W)&0x10000? C_FLAG:0); \
  R->Rg1.W=J.W

/*
  ADD Rg1, Rg2 (16 bits).
  - N se pone a 0.
  - H se calcula con XOR de bits 12.
  - C se calcula con la suma en long para detectar bit 16.
*/

#define M_ADCW(Rg)      \
  I=R->AF.B.l&C_FLAG;J.W=(R->HL.W+R->Rg.W+I)&0xFFFF;           \
  R->AF.B.l=                                                   \
    (((long)R->HL.W+(long)R->Rg.W+(long)I)&0x10000? C_FLAG:0)| \
    (~(R->HL.W^R->Rg.W)&(R->Rg.W^J.W)&0x8000? V_FLAG:0)|       \
    ((R->HL.W^R->Rg.W^J.W)&0x1000? H_FLAG:0)|                  \
    (J.W? 0:Z_FLAG)|(J.B.h&S_FLAG);                            \
  R->HL.W=J.W

// ADC HL, Rg (16 bits con carry). Similar a ADDW pero con V y Z/S.

#define M_SBCW(Rg)      \
  I=R->AF.B.l&C_FLAG;J.W=(R->HL.W-R->Rg.W-I)&0xFFFF;           \
  R->AF.B.l=                                                   \
    N_FLAG|                                                    \
    (((long)R->HL.W-(long)R->Rg.W-(long)I)&0x10000? C_FLAG:0)| \
    ((R->HL.W^R->Rg.W)&(R->HL.W^J.W)&0x8000? V_FLAG:0)|        \
    ((R->HL.W^R->Rg.W^J.W)&0x1000? H_FLAG:0)|                  \
    (J.W? 0:Z_FLAG)|(J.B.h&S_FLAG);                            \
  R->HL.W=J.W

// SBC HL, Rg (16 bits con borrow). N_FLAG activado.

/* ==========================================================================
   ENUMS DE OPCODES
   El Z80 tiene 256 opcodes principales (enum Codes), y otros 256 para el
   prefijo CB (enum CodesCB), y otros 256 para ED (enum CodesED).
   El orden de los enums DEBE coincidir con el orden de la tabla Cycles[]
   de Tables.h, porque se usan como índice.
   ========================================================================== */

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

/*
  Los enums son listas de los 256 opcodes de cada grupo. Cada nombre
  identifica una instrucción. "DB_xx" son "define byte": opcodes no
  documentados o reservados (no hacen nada o están definidos como NOP).
  El orden DEBE coincidir con la tabla Cycles[] de Tables.h, porque se
  usan como índice.
*/

/* ==========================================================================
   DESPACHO DE OPCODES DEL GRUPO CB (RLC/RRC/RL/RR/SLA/SRA/SLL/SRL/BIT/SET/RES)
   ========================================================================== */

static void CodesCB(register Z80 *R)
{
  register byte I;

  I=OpZ80(R->PC.W++);       // Leer el opcode CB.
  R->ICount-=CyclesCB[I];   // Descontar los ciclos de esa instrucción.
  switch(I)
  {
#include "CodesCB.h"        // Tabla de "case" con el código de cada opcode.
    default:
      if(R->TrapBadOps)     // Si el opcode no está implementado y estamos en modo debug...
        printf
        (
          "[Z80 %lX] Unrecognized instruction: CB %02X at PC=%04X\n",
          (long)(R->User),OpZ80(R->PC.W-1),R->PC.W-2
        );
  }
}

/*
  CodesCB.h contiene las implementaciones de las instrucciones con prefijo CB.
  Cada "case RLC_B:" aplica la macro correspondiente (M_RLC, etc.).
*/

static void CodesDDCB(register Z80 *R)
{
  register pair J;
  register byte I;

#define XX IX
  J.W=R->XX.W+(offset)OpZ80(R->PC.W++);  // Dirección = IX + offset (8 bits con signo).
  I=OpZ80(R->PC.W++);                     // Leer el opcode CB real.
  R->ICount-=CyclesXXCB[I];               // Ciclos (tabla especial para DD CB).
  switch(I)
  {
#include "CodesXCB.h"        // Tabla compartida con FD CB.
    default:
      if(R->TrapBadOps)
        printf
        (
          "[Z80 %lX] Unrecognized instruction: DD CB %02X %02X at PC=%04X\n",
          (long)(R->User),OpZ80(R->PC.W-2),OpZ80(R->PC.W-1),R->PC.W-4
        );
  }
#undef XX
}

static void CodesFDCB(register Z80 *R)
{
  register pair J;
  register byte I;

#define XX IY
  J.W=R->XX.W+(offset)OpZ80(R->PC.W++);   // Igual que DDCB pero con IY.
  I=OpZ80(R->PC.W++);
  R->ICount-=CyclesXXCB[I];
  switch(I)
  {
#include "CodesXCB.h"
    default:
      if(R->TrapBadOps)
        printf
        (
          "[Z80 %lX] Unrecognized instruction: FD CB %02X %02X at PC=%04X\n",
          (long)R->User,OpZ80(R->PC.W-2),OpZ80(R->PC.W-1),R->PC.W-4
        );
  }
#undef XX
}

/* ==========================================================================
   DESPACHO DE OPCODES DEL GRUPO ED (I/O extendido, bloques, etc.)
   ========================================================================== */

static void CodesED(register Z80 *R)
{
  register byte I;
  register pair J;

  I=OpZ80(R->PC.W++);
  R->ICount-=CyclesED[I];
  switch(I)
  {
#include "CodesED.h"
    case PFX_ED:              // Prefijo ED repetido: ignorar el segundo.
      R->PC.W--;break;
    default:
      if(R->TrapBadOps)
        printf
        (
          "[Z80 %lX] Unrecognized instruction: ED %02X at PC=%04X\n",
          (long)R->User,OpZ80(R->PC.W-1),R->PC.W-2
        );
  }
}

/* ==========================================================================
   DESPACHO DE OPCODES DEL GRUPO DD (registros IX/IY)
   ========================================================================== */

static void CodesDD(register Z80 *R)
{
  register byte I;
  register pair J;

#define XX IX
  I=OpZ80(R->PC.W++);
  R->ICount-=CyclesXX[I];
  switch(I)
  {
#include "CodesXX.h"
    case PFX_FD:              // DD seguido de FD: ignorar el DD.
    case PFX_DD:              // DD DD: ignorar el primero.
      R->PC.W--;break;
    case PFX_CB:              // DD CB: instrucción con IX y offset.
      CodesDDCB(R);break;
    default:
      if(R->TrapBadOps)
        printf
        (
          "[Z80 %lX] Unrecognized instruction: DD %02X at PC=%04X\n",
          (long)R->User,OpZ80(R->PC.W-1),R->PC.W-2
        );
  }
#undef XX
}

static void CodesFD(register Z80 *R)
{
  register byte I;
  register pair J;

#define XX IY
  I=OpZ80(R->PC.W++);
  R->ICount-=CyclesXX[I];
  switch(I)
  {
#include "CodesXX.h"
    case PFX_FD:
    case PFX_DD:
      R->PC.W--;break;
    case PFX_CB:
      CodesFDCB(R);break;
    default:
        printf
        (
          "Unrecognized instruction: FD %02X at PC=%04X\n",
          OpZ80(R->PC.W-1),R->PC.W-2
        );
  }
#undef XX
}

/* ==========================================================================
   ResetZ80: inicializa los registros a sus valores de reset
   ========================================================================== */

void ResetZ80(Z80 *R)
{
  R->PC.W     = 0x0000;       // Program Counter.
  R->SP.W     = 0xF000;       // Stack Pointer (empieza arriba).
  R->AF.W     = 0x0000;
  R->BC.W     = 0x0000;
  R->DE.W     = 0x0000;
  R->HL.W     = 0x0000;
  R->AF1.W    = 0x0000;       // Registros alternativos (EXX).
  R->BC1.W    = 0x0000;
  R->DE1.W    = 0x0000;
  R->HL1.W    = 0x0000;
  R->IX.W     = 0x0000;       // Registros índice.
  R->IY.W     = 0x0000;
  R->I        = 0x00;         // Registro de vector de interrupción.
  R->R        = 0x00;         // Contador de refresh.
  R->IFF      = 0x00;         // Flip-flops de interrupción.
  R->ICount   = R->IPeriod;   // Ciclos restantes.
  R->IRequest = INT_NONE;     // Sin interrupción pendiente.
  R->IBackup  = 0;

  JumpZ80(R->PC.W);           // Notifica al emulador del salto inicial.
}

/* ==========================================================================
   ExecZ80: ejecuta un número de ciclos (solo si EXECZ80 está definido)
   Galagino NO la usa. Es una alternativa a RunZ80() para ejecutar un
   número fijo de ciclos y salir.
   ========================================================================== */

#ifdef EXECZ80
int ExecZ80(register Z80 *R,register int RunCycles)
{
  register byte I;
  register pair J;

  for(R->ICount=RunCycles;;)
  {
    while(R->ICount>0)
    {
#ifdef DEBUG
      if(R->PC.W==R->Trap) R->Trace=1;
      if(R->Trace)
        if(!DebugZ80(R)) return(R->ICount);
#endif

      I=OpZ80(R->PC.W++);       // Leer opcode.
      R->ICount-=Cycles[I];     // Descontar ciclos.

      switch(I)
      {
#include "Codes.h"
        case PFX_CB: CodesCB(R);break;
        case PFX_ED: CodesED(R);break;
        case PFX_FD: CodesFD(R);break;
        case PFX_DD: CodesDD(R);break;
      }
    }

    if(!(R->IFF&IFF_EI)) return(R->ICount);
    else
    {
      R->IFF=(R->IFF&~IFF_EI)|IFF_1;
      R->ICount+=R->IBackup-1;
      if((R->IRequest!=INT_NONE)&&(R->IRequest!=INT_QUIT)) IntZ80(R,R->IRequest);
    }
  }
}
#endif /* EXECZ80 */

/* ==========================================================================
   IntZ80: genera una interrupción
   ========================================================================== */

void IntZ80(Z80 *R,z80_word Vector)
{
  // Si el CPU estaba en HALT, salir de él.
  if(R->IFF&IFF_HALT) { R->PC.W++;R->IFF&=~IFF_HALT; }

  if((R->IFF&IFF_1)||(Vector==INT_NMI))
  {
    // Guardar PC en la pila.
    M_PUSH(PC);

    // Auto-reset de la petición de interrupción.
    if(R->IAutoReset&&(Vector==R->IRequest)) R->IRequest=INT_NONE;

    // Si es NMI (Non-Maskable Interrupt)...
    if(Vector==INT_NMI)
    {
      R->IFF&=~(IFF_1|IFF_EI);   // Deshabilitar interrupciones.
      R->PC.W=0x0066;            // Vector fijo de NMI.
      JumpZ80(0x0066);
      return;
    }

    // Deshabilitar más interrupciones.
    R->IFF&=~(IFF_1|IFF_2|IFF_EI);

    // Modo IM2: vector indirecto.
    if(R->IFF&IFF_IM2)
    {
      Vector=(Vector&0xFF)|((z80_word)(R->I)<<8);  // Vector = (I << 8) | dato.
      R->PC.B.l=RdZ80(Vector++);
      R->PC.B.h=RdZ80(Vector);
      JumpZ80(R->PC.W);
      return;
    }

    // Modo IM1: vector fijo 0x0038.
    if(R->IFF&IFF_IM1) { R->PC.W=0x0038;JumpZ80(0x0038);return; }

    // Modo IM0: vector pasado por el bus de datos.
    switch(Vector)
    {
      case INT_RST00: R->PC.W=0x0000;JumpZ80(0x0000);break;
      case INT_RST08: R->PC.W=0x0008;JumpZ80(0x0008);break;
      case INT_RST10: R->PC.W=0x0010;JumpZ80(0x0010);break;
      case INT_RST18: R->PC.W=0x0018;JumpZ80(0x0018);break;
      case INT_RST20: R->PC.W=0x0020;JumpZ80(0x0020);break;
      case INT_RST28: R->PC.W=0x0028;JumpZ80(0x0028);break;
      case INT_RST30: R->PC.W=0x0030;JumpZ80(0x0030);break;
      case INT_RST38: R->PC.W=0x0038;JumpZ80(0x0038);break;
    }
  }
}

/* ==========================================================================
   RunZ80: ejecuta hasta que LoopZ80 devuelva INT_QUIT
   Galagino NO la usa. Es el modo "clásico" del emulador.
   ========================================================================== */

#ifndef EXECZ80
z80_word RunZ80(Z80 *R)
{
  register byte I;
  register pair J;

  for(;;)
  {
#ifdef DEBUG
    if(R->PC.W==R->Trap) R->Trace=1;
    if(R->Trace)
      if(!DebugZ80(R)) return(R->PC.W);
#endif

    I=OpZ80(R->PC.W++);
    R->ICount-=Cycles[I];

    switch(I)
    {
#include "Codes.h"
      case PFX_CB: CodesCB(R);break;
      case PFX_ED: CodesED(R);break;
      case PFX_FD: CodesFD(R);break;
      case PFX_DD: CodesDD(R);break;
    }

    // Si se acaban los ciclos, llamar a LoopZ80 para gestionar IRQ.
    if(R->ICount<=0)
    {
      if(R->IFF&IFF_EI)
      {
        R->IFF=(R->IFF&~IFF_EI)|IFF_1;
        R->ICount+=R->IBackup-1;

        if(R->ICount>0) J.W=R->IRequest;
        else
        {
          J.W=LoopZ80(R);
          R->ICount+=R->IPeriod;
          if(J.W==INT_NONE) J.W=R->IRequest;
        }
      }
      else
      {
        J.W=LoopZ80(R);
        R->ICount+=R->IPeriod;
        if(J.W==INT_NONE) J.W=R->IRequest;
      }

      if(J.W==INT_QUIT) return(R->PC.W);
      if(J.W!=INT_NONE) IntZ80(R,J.W);
    }
  }

  return(R->PC.W);
}
#endif /* !EXECZ80 */

/* ==========================================================================
   StepZ80: ejecuta UNA instrucción
   ESTA es la función que usa Galagino. Se llama desde los bucles de
   emulación de cada juego (X_run_frame) un número fijo de veces por frame.
   ========================================================================== */

void StepZ80(Z80 *R)
{
  register byte I;
  register pair J;

  I=OpZ80_INL(R->PC.W++);   // Leer opcode usando la versión inline de emulation.h.

  switch(I)
  {
#include "Codes.h"
    case PFX_CB: CodesCB(R);break;
    case PFX_ED: CodesED(R);break;
    case PFX_FD: CodesFD(R);break;
    case PFX_DD: CodesDD(R);break;
  }

  // Si venimos de un EI, activar IFF1.
  if(R->IFF&IFF_EI)
    R->IFF=(R->IFF&~IFF_EI)|IFF_1;
}
