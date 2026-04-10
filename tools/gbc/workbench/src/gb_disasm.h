/*
 * gb_disasm.h — Game Boy Z80 disassembler
 */
#pragma once
#include <stdint.h>

typedef uint8_t (*GbDisasmRead)(uint16_t addr);
int gb_disasm(uint16_t addr, char *buf, int buf_len, GbDisasmRead read);
