/*
 * debug.h — Software breakpoints for QL Studio emulator
 *
 * Write a marker byte (1-255) to address 0x3FFFE.
 * The emulator detects it, pauses, and shows which breakpoint hit.
 * Zero cost on real hardware (writes to unused RAM).
 *
 * Usage:
 *   #include "debug.h"
 *
 *   BREAK(1);           // pause with "SOFTWARE BP #1"
 *   BREAK(2);           // pause with "SOFTWARE BP #2"
 *   BREAK_IF(x > 100);  // conditional break
 */

#ifndef DEBUG_H
#define DEBUG_H

#define DEBUG_BP_ADDR  0x3FFFEL

#define BREAK(n)       do { *((volatile unsigned char *)DEBUG_BP_ADDR) = (n); } while(0)
#define BREAK_IF(cond) do { if (cond) BREAK(1); } while(0)

#endif
