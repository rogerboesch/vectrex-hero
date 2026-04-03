/*
 * m68k_disasm.h — Compact Motorola 68000 disassembler
 */
#pragma once

#include <stdint.h>

/* Disassemble one instruction at addr.
   Returns the instruction size in bytes (2, 4, 6, ...).
   Writes the mnemonic + operands into buf (max buf_len chars).
   read_word(addr) is called to fetch instruction words. */
typedef uint16_t (*DisasmReadWord)(uint32_t addr);

int m68k_disasm(uint32_t addr, char *buf, int buf_len, DisasmReadWord read_word);
