/*
 * m6809_disasm.h — Compact 6809 disassembler
 */
#pragma once

typedef unsigned char (*Disasm6809Read)(unsigned addr);

/* Disassemble one instruction. Returns size in bytes. */
int m6809_disasm(unsigned addr, char *buf, int buf_len, Disasm6809Read read);
