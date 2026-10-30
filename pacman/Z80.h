/** Z80: portable Z80 emulator *******************************/
/**                                                         **/
/**                           Z80.h                         **/
/**                                                         **/
/** This file contains declarations relevant to emulation   **/
/** of Z80 CPU.                                             **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-2007                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/   
/**     changes to this file.                               **/
/*************************************************************/

/*
  ============================================================================
  Z80.h — cabecera del emulador de CPU Z80
  ============================================================================
  Declara:
    - Constantes de interrupción (INT_*).
    - Bits del registro F (S_FLAG, Z_FLAG, ...).
    - Bits del registro IFF (IFF_1, IFF_EI, ...).
    - Tipos básicos: byte, word, offset.
    - El tipo pair (par de bytes / word).
    - La estructura Z80 (todos los registros + estado del emulador).
    - Las funciones que el emulador Z80 llama y que el proyecto debe
      implementar: RdZ80, WrZ80, InZ80, OutZ80, LoopZ80, PatchZ80, JumpZ80.
  ============================================================================
*/

#ifndef Z80_H
#define Z80_H

/*
  Guarda de inclusión: evita que el header se procese dos veces si varios
  archivos lo incluyen. Necesario porque la estructura Z80 y las funciones
  no deben declararse dos veces.
*/

#ifdef __cplusplus
extern "C" {
#endif

/*
  `extern "C"` para que las funciones se puedan llamar desde código C
  (el emulador Z80 se compila como C, no como C++). Sin esto, el compilador
  de C++ aplicaría "name mangling" y el enlazador no encontraría las
  funciones desde los archivos .c del emulador.
*/

                               /* Compilation options:       */
/* #define DEBUG */            /* Compile debugging version  */
#define LSB_FIRST              /* Compile for low-endian CPU */
/* #define MSB_FIRST */        /* Compile for hi-endian CPU  */

/*
  Opciones de compilación:
    - DEBUG: compila el emulador con soporte de depuración (DebugZ80, Trace).
      Comentado por defecto → sin debug, más rápido.
    - LSB_FIRST: define el orden de bytes como little-endian. El ESP32 es
      little-endian, así que SIEMPRE debe estar definido. Si se cambia a
      MSB_FIRST en una máquina big-endian, los pares de bytes (pair) se
      leerían al revés.
*/

                               /* LoopZ80() may return:      */
#define INT_RST00   0x00C7     /* RST 00h                    */
#define INT_RST08   0x00CF     /* RST 08h                    */
#define INT_RST10   0x00D7     /* RST 10h                    */
#define INT_RST18   0x00DF     /* RST 18h                    */
#define INT_RST20   0x00E7     /* RST 20h                    */
#define INT_RST28   0x00EF     /* RST 28h                    */
#define INT_RST30   0x00F7     /* RST 30h                    */
#define INT_RST38   0x00FF     /* RST 38h                    */
#define INT_IRQ     INT_RST38  /* Default IRQ opcode is FFh  */
#define INT_NMI     0xFFFD     /* Non-maskable interrupt     */
#define INT_NONE    0xFFFF     /* No interrupt required      */
#define INT_QUIT    0xFFFE     /* Exit the emulation         */

/*
  Valores que LoopZ80() puede devolver al emulador:
    - INT_RSTxx: petición de interrupción con vector RST xx. Los valores
      0x00C7, 0x00CF, etc. son los opcodes del Z80 para RST 00h, RST 08h...
      El emulador los interpreta como "salta a la dirección 0x00, 0x08...".
    - INT_IRQ: valor por defecto (RST 38h, el más común en máquinas arcade).
    - INT_NMI: interrupción no enmascarable (NMI). Va al vector fijo 0x0066.
    - INT_NONE: no hay interrupción pendiente.
    - INT_QUIT: señal para salir del bucle RunZ80() (no usado en Galagino).
*/

                               /* Bits in Z80 F register:    */
#define S_FLAG      0x80       /* 1: Result negative         */
#define Z_FLAG      0x40       /* 1: Result is zero          */
#define H_FLAG      0x10       /* 1: Halfcarry/Halfborrow    */
#define P_FLAG      0x04       /* 1: Result is even          */
#define V_FLAG      0x04       /* 1: Overflow occured        */
#define N_FLAG      0x02       /* 1: Subtraction occured     */
#define C_FLAG      0x01       /* 1: Carry/Borrow occured    */

/*
  Bits del registro F (flags) del Z80. Cada bit indica una condición:
    - S (Sign): el resultado es negativo (bit 7 a 1).
    - Z (Zero): el resultado es cero.
    - H (Half-carry): acarreo del bit 3 al 4 (útil para DAA).
    - P/V (Parity/Overflow): según la instrucción, indica paridad par
      o desbordamiento aritmético. Comparten el mismo bit (0x04).
    - N (Add/Sub): 1 si la última operación fue una resta.
    - C (Carry): acarreo del bit 7 (o borrow en restas).
  En Galagino, estos defines se usan en las macros de flags de Z80.c.
*/

                               /* Bits in IFF flip-flops:    */
#define IFF_1       0x01       /* IFF1 flip-flop             */
#define IFF_IM1     0x02       /* 1: IM1 mode                */
#define IFF_IM2     0x04       /* 1: IM2 mode                */
#define IFF_2       0x08       /* IFF2 flip-flop             */
#define IFF_EI      0x20       /* 1: EI pending              */
#define IFF_HALT    0x80       /* 1: CPU HALTed              */

/*
  Bits del registro IFF (Interrupt Flip-Flop) del Z80:
    - IFF_1: habilita/deshabilita interrupciones enmascarables.
    - IFF_IM1 / IFF_IM2: modo de interrupción (IM1 o IM2).
    - IFF_2: copia de IFF1, usada por RETN.
    - IFF_EI: hay un EI pendiente (el Z80 retrasa las interrupciones
      una instrucción después de EI).
    - IFF_HALT: el CPU está en estado HALT (ejecutando NOP hasta
      que llegue una interrupción).
*/

/** Simple Datatypes *****************************************/
/** NOTICE: sizeof(byte)=1 and sizeof(z80_word)=2           **/
/*************************************************************/
#ifndef BYTE_TYPE_DEFINED
#define BYTE_TYPE_DEFINED
typedef unsigned char byte;
#endif
#ifndef Z80_WORD_TYPE_DEFINED
#define Z80_WORD_TYPE_DEFINED
typedef unsigned short z80_word;
#endif
typedef signed char offset;

/*
  Tipos básicos del emulador:
    - byte: 1 byte sin signo (registros de 8 bits del Z80).
    - z80_word: 2 bytes sin signo (registros de 16 bits del Z80).
    - offset: 1 byte CON signo (para desplazamientos relativos de JR).
  Las guardas BYTE_TYPE_DEFINED y Z80_WORD_TYPE_DEFINED evitan "redefinición"
  si otro header del proyecto ya ha definido byte o z80_word (por ejemplo, un
  "typedef unsigned char byte;" en config.h).
*/

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
  z80_word W;
} pair;

/*
  Tipo "pair": un par de bytes que también se puede ver como un word.
  Es una UNION: los dos campos comparten la misma memoria.
    - .W → el valor de 16 bits.
    - .B.l y .B.h → los bytes bajo y alto.
  En little-endian (ESP32), .B.l ocupa la dirección más baja (primer byte).
  Ejemplo: si .W = 0x1234, entonces .B.l = 0x34 y .B.h = 0x12.
  Esto permite acceder a los registros del Z80 de las dos formas (8 y 16
  bits) sin conversiones: R->AF.W = 0x1234; o R->AF.B.h = 0x12; R->AF.B.l = 0x34.
*/

typedef struct
{
  pair AF,BC,DE,HL,IX,IY,PC,SP;       /* Main registers      */
  pair AF1,BC1,DE1,HL1;               /* Shadow registers    */
  byte IFF,I;                         /* Interrupt registers */
  byte R;                             /* Refresh register    */

  int IPeriod,ICount; /* Set IPeriod to number of CPU cycles */
                      /* between calls to LoopZ80()          */
  int IBackup;        /* Private, don't touch                */
  z80_word IRequest;  /* Set to address of pending IRQ       */
  byte IAutoReset;    /* Set to 1 to autom. reset IRequest   */
  byte TrapBadOps;    /* Set to 1 to warn of illegal opcodes */
  z80_word Trap;      /* Set Trap to address to trace from   */
  byte Trace;         /* Set Trace=1 to start tracing        */
  void *User;         /* Arbitrary user data (ID,RAM*,etc.)  */
} Z80;

/*
  Estructura Z80: contiene TODOS los registros del CPU y el estado del
  emulador.

  Registros principales (pares de 8/16 bits):
    - AF: acumulador (A) y flags (F). A es el registro de 8 bits más usado.
    - BC, DE, HL: registros de propósito general. HL es el "puntero" por
      excelencia (acceso a memoria indirecto).
    - IX, IY: registros índice (para acceso con desplazamiento).
    - PC: Program Counter (dirección de la siguiente instrucción).
    - SP: Stack Pointer (puntero de pila, para PUSH/POP/CALL/RET).

  Registros alternativos (shadow):
    - AF1, BC1, DE1, HL1: copias de los principales. La instrucción EXX
      los intercambia con los principales de golpe.

  Registros de interrupción:
    - IFF: bits de control de interrupciones (ver defines más arriba).
    - I: registro de vector de interrupción (para IM2).
    - R: registro de refresh de memoria dinámica.

  Estado del emulador (no son registros del Z80 real):
    - IPeriod: ciclos de CPU entre llamadas a LoopZ80.
    - ICount: ciclos restantes en el bloque actual.
    - IBackup: uso interno (para el retraso del EI).
    - IRequest: interrupción pendiente que se debe atender.
    - IAutoReset: si 1, IRequest se limpia tras atenderla.
    - TrapBadOps: si 1, imprime un mensaje al encontrar opcodes no
      implementados (útil para debug).
    - Trap: dirección en la que activar el trace automáticamente.
    - Trace: si 1, llama a DebugZ80 tras cada instrucción.
    - User: puntero libre para que el proyecto guarde sus datos.
      En Galagino, apunta a la RAM de la máquina emulada.
*/

/** ResetZ80() ***********************************************/
/** This function can be used to reset the registers before **/
/** starting execution with RunZ80(). It sets registers to  **/
/** their initial values.                                   **/
/*************************************************************/
void ResetZ80(register Z80 *R);

/*
  ResetZ80: inicializa todos los registros a sus valores de reset.
  Se llama desde prepare_emulation() para cada una de las 3 CPUs.
  "register" es una sugerencia al compilador (en ESP32 se ignora).
*/

/** ExecZ80() ************************************************/
/** This function will execute given number of Z80 cycles.  **/
/** It will then return the number of cycles left, possibly **/
/** negative, and current register values in R.             **/
/*************************************************************/
#ifdef EXECZ80
int ExecZ80(register Z80 *R,register int RunCycles);
#endif

/*
  ExecZ80: ejecuta un número exacto de ciclos y devuelve los que sobran.
  Solo se declara si EXECZ80 está definido. Galagino NO usa esta función
  (usa StepZ80). El bloque está protegido con #ifdef para no declararla
  si no se va a definir.
*/

/** IntZ80() *************************************************/
/** This function will generate interrupt of given vector.  **/
/*************************************************************/
void IntZ80(register Z80 *R,register z80_word Vector);

/*
  IntZ80: genera una interrupción con el vector indicado.
  - Vector puede ser INT_NMI, INT_RSTxx, etc.
  - Internamente guarda PC en la pila y salta al vector correspondiente
    según el modo de interrupción (IM0, IM1, IM2).
  La llama el emulador Z80 en RunZ80, o el proyecto directamente cuando
  necesita provocar una interrupción durante el refresco de vídeo.
*/

/** RunZ80() *************************************************/
/** This function will run Z80 code until an LoopZ80() call **/
/** returns INT_QUIT. It will return the PC at which        **/
/** emulation stopped, and current register values in R.    **/
/*************************************************************/
#ifndef EXECZ80
z80_word RunZ80(register Z80 *R);
#endif

void StepZ80(register Z80 *R);

/*
  RunZ80: ejecuta código Z80 hasta que LoopZ80 devuelva INT_QUIT.
  Solo se declara si EXECZ80 NO está definido (son excluyentes).
  Galagino NO usa RunZ80 (usa StepZ80 en su lugar), pero la declaración
  existe porque el archivo Z80.c la define (aunque el enlazador la puede
  eliminar si no se llama).
*/

/** RdZ80()/WrZ80() ******************************************/
/** These functions are called when access to RAM occurs.   **/
/** They allow to control memory access.                    **/
/************************************ TO BE WRITTEN BY USER **/
void WrZ80(register z80_word Addr,register byte Value);
byte RdZ80(register z80_word Addr);

/*
  RdZ80/WrZ80: lectura y escritura de memoria.
  Las implementa el proyecto (en Galagino, emulation.c). La CPU Z80 las
  llama cada vez que necesita leer/escribir un byte en memoria.
  - RdZ80(addr): devuelve el byte en la dirección addr.
  - WrZ80(addr, value): escribe value en addr.
  En Galagino, RdZ80/WrZ80 delegan en el mapa de memoria de Pac-Man.
*/

/** InZ80()/OutZ80() *****************************************/
/** Z80 emulation calls these functions to read/write from  **/
/** I/O ports. There can be 65536 I/O ports, but only first **/
/** 256 are usually used.                                   **/
/************************************ TO BE WRITTEN BY USER **/
void OutZ80(register z80_word Port,register byte Value);
byte InZ80(register z80_word Port);

/*
  InZ80/OutZ80: lectura y escritura de puertos de E/S.
  Las implementa el proyecto. El Z80 tiene 64 KB de espacio de puertos,
  pero en la práctica los juegos arcade usan los primeros 256.
  Pac-Man no utiliza los puertos de entrada.
*/

/** PatchZ80() ***********************************************/
/** Z80 emulation calls this function when it encounters a  **/
/** special patch command (ED FE) provided for user needs.  **/
/** For example, it can be called to emulate BIOS calls,    **/
/** such as disk and tape access. Replace it with an empty  **/
/** macro for no patching.                                  **/
/************************************ TO BE WRITTEN BY USER **/
void PatchZ80(register Z80 *R);

/*
  PatchZ80: hook para interceptar el opcode "ED FE" (que en el Z80 real
  no hace nada). El emulador la llama cuando encuentra ese opcode, y el
  proyecto puede usarla para emular llamadas al BIOS, acceso a disco, etc.
  En Galagino, PatchZ80 es una función VACÍA (no se usa).
*/

/** DebugZ80() ***********************************************/
/** This function should exist if DEBUG is #defined. When   **/
/** Trace!=0, it is called after each command executed by   **/
/** the CPU, and given the Z80 registers. Emulation exits   **/
/** if DebugZ80() returns 0.                                **/
/*************************************************************/
#ifdef DEBUG
byte DebugZ80(register Z80 *R);
#endif

/*
  DebugZ80: solo se declara si DEBUG está definido. Se llama tras cada
  instrucción cuando Trace=1, y permite inspeccionar los registros. Si
  devuelve 0, la emulación se detiene.
  En Galagino, DEBUG no está definido, así que no existe.
*/

/** LoopZ80() ************************************************/
/** Z80 emulation calls this function periodically to check **/
/** if the system hardware requires any interrupts. This    **/
/** function must return an address of the interrupt vector **/
/** (0x0038, 0x0066, etc.) or INT_NONE for no interrupt.    **/
/** Return INT_QUIT to exit the emulation loop.             **/
/************************************ TO BE WRITTEN BY USER **/
z80_word LoopZ80(register Z80 *R);

/*
  LoopZ80: se llama periódicamente (cada IPeriod ciclos) para que el
  proyecto gestione interrupciones y temporización. Devuelve:
    - INT_NONE: no hay interrupción.
    - INT_RSTxx / INT_NMI: hay interrupción con ese vector.
    - INT_QUIT: salir del bucle (solo en RunZ80).
  En Galagino, LoopZ80 NO se usa porque no se usa RunZ80 (se usa StepZ80).
  Pero la función debe existir para que el enlazador no se queje (o no se
  enlaza si nadie la llama).
*/

/** JumpZ80() ************************************************/
/** Z80 emulation calls this function when it executes a    **/
/** JP, JR, CALL, RST, or RET. You can use JumpZ80() to     **/
/** trap these opcodes and switch memory layout.            **/
/************************************ TO BE WRITTEN BY USER **/
#ifndef JUMPZ80
#define JumpZ80(PC)
#else
void JumpZ80(z80_word PC);
#endif

/*
  JumpZ80: hook que se llama cada vez que el Z80 salta (JP, JR, CALL,
  RST, RET). Permite interceptar los saltos para cambiar el mapa de
  memoria (útil en sistemas con paginación).
  Por defecto: JUMPZ80 NO está definido, así que JumpZ80 se resuelve a
  una MACRO VACÍA. Esto significa que todas las llamadas a JumpZ80(PC)
  en Z80.c se compilan como "no hacer nada" → cero coste.
  Si el proyecto necesita interceptar saltos, define JUMPZ80 y proporciona
  la función. Galagino NO la define, así que JumpZ80 es un no-op.
*/

#ifdef __cplusplus
}
#endif
#endif /* Z80_H */

/*
  Cierra el extern "C" (si estábamos en C++) y la guarda de inclusión.
  A partir de aquí, el código vuelve al modo normal de C++.
*/

#include "config.h"
#include "emulation.h"

/*
  ATENCIÓN: estos dos #include están FUERA de la guarda de inclusión
  (#endif /* Z80_H  ya se ha cerrado). Eso significa que cada vez
  que alguien haga #include "Z80.h", se volverán a incluir config.h y
  emulation.h. En la práctica no rompe nada porque ambos tienen sus
  propias guardas de inclusión (#ifndef _CONFIG_H_ y #ifndef _EMULATION_H_),
  pero es un patrón poco habitual. Probablemente el autor los puso aquí
  para asegurarse de que, al incluir Z80.h, también se incluyan las
  definiciones del proyecto (necesarias para que Z80.c compile).
  
  Lo más limpio sería moverlos dentro de la guarda (antes del #endif),
  pero eso cambiaría el comportamiento original. Como el código funciona
  tal cual, se dejan donde están.
*/
