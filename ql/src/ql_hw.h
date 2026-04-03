/*
 * ql_hw.h — Sinclair QL hardware abstraction
 *
 * QDOS trap interfaces exposed as C functions.
 * Implemented in ql_hw.s (assembly) for trap-level access.
 */

#ifndef QL_HW_H
#define QL_HW_H

#include "game.h"

/* QDOS system variable base (Minerva/QDOS) */
#define SV_BASE     0x28000L

/* Key scan codes (QDOS IPC keyboard matrix) */
#define QL_KEY_Q      0x10
#define QL_KEY_A      0x20
#define QL_KEY_O      0x30
#define QL_KEY_P      0x38
#define QL_KEY_SPACE  0xF4
#define QL_KEY_ENTER  0xF8
#define QL_KEY_ESC    0xF7
#define QL_KEY_D      0x28
#define QL_KEY_UP     0xC8
#define QL_KEY_DOWN   0xD0
#define QL_KEY_LEFT   0xC0
#define QL_KEY_RIGHT  0xD8

#endif
