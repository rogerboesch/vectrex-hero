/*
 * debug.h — Software breakpoint macros for Vectrex Studio
 *
 * The emulator checks address $CFFF each frame.
 * If non-zero, it pauses and reports the marker value.
 *
 * Usage:
 *   BREAK(1);           // unconditional breakpoint #1
 *   BREAK_IF(hp == 0);  // conditional breakpoint
 */
#ifndef DEBUG_H
#define DEBUG_H

#define SOFT_BP_ADDR    ((volatile unsigned char *)0xCFFF)
#define BREAK(n)        (*SOFT_BP_ADDR = (n))
#define BREAK_IF(cond)  do { if (cond) BREAK(1); } while(0)

#endif
