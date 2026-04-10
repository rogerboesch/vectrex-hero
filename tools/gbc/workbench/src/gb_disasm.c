/*
 * gb_disasm.c — Compact Game Boy (LR35902) disassembler
 */
#include "gb_disasm.h"
#include <stdio.h>

static GbDisasmRead g_rd;
static uint16_t g_pc;
static uint8_t fetch(void) { return g_rd(g_pc++); }
static uint16_t fetch16(void) { uint8_t lo = fetch(); return lo | ((uint16_t)fetch() << 8); }
static int8_t sign8(uint8_t b) { return (int8_t)b; }

static const char *r8[] = {"B","C","D","E","H","L","(HL)","A"};
static const char *r16[] = {"BC","DE","HL","SP"};
static const char *r16af[] = {"BC","DE","HL","AF"};
static const char *cc[] = {"NZ","Z","NC","C"};
static const char *alu[] = {"ADD A,","ADC A,","SUB","SBC A,","AND","XOR","OR","CP"};

int gb_disasm(uint16_t addr, char *buf, int buf_len, GbDisasmRead read) {
    g_rd = read; g_pc = addr;
    uint8_t op = fetch();

    /* NOP, HALT, STOP, DI, EI, etc. */
    switch (op) {
    case 0x00: snprintf(buf, buf_len, "NOP"); return g_pc - addr;
    case 0x10: fetch(); snprintf(buf, buf_len, "STOP"); return g_pc - addr;
    case 0x76: snprintf(buf, buf_len, "HALT"); return g_pc - addr;
    case 0xF3: snprintf(buf, buf_len, "DI"); return g_pc - addr;
    case 0xFB: snprintf(buf, buf_len, "EI"); return g_pc - addr;
    case 0xC9: snprintf(buf, buf_len, "RET"); return g_pc - addr;
    case 0xD9: snprintf(buf, buf_len, "RETI"); return g_pc - addr;
    case 0x27: snprintf(buf, buf_len, "DAA"); return g_pc - addr;
    case 0x2F: snprintf(buf, buf_len, "CPL"); return g_pc - addr;
    case 0x37: snprintf(buf, buf_len, "SCF"); return g_pc - addr;
    case 0x3F: snprintf(buf, buf_len, "CCF"); return g_pc - addr;
    case 0x07: snprintf(buf, buf_len, "RLCA"); return g_pc - addr;
    case 0x0F: snprintf(buf, buf_len, "RRCA"); return g_pc - addr;
    case 0x17: snprintf(buf, buf_len, "RLA"); return g_pc - addr;
    case 0x1F: snprintf(buf, buf_len, "RRA"); return g_pc - addr;
    case 0xE9: snprintf(buf, buf_len, "JP   (HL)"); return g_pc - addr;
    case 0xF9: snprintf(buf, buf_len, "LD   SP,HL"); return g_pc - addr;
    }

    /* LD r8,r8 and HALT (0x40-0x7F) */
    if (op >= 0x40 && op <= 0x7F && op != 0x76) {
        snprintf(buf, buf_len, "LD   %s,%s", r8[(op>>3)&7], r8[op&7]);
        return g_pc - addr;
    }

    /* ALU A,r8 (0x80-0xBF) */
    if (op >= 0x80 && op <= 0xBF) {
        snprintf(buf, buf_len, "%-5s%s", alu[(op>>3)&7], r8[op&7]);
        return g_pc - addr;
    }

    /* LD r8,n8 */
    if ((op & 0xC7) == 0x06) {
        uint8_t n = fetch();
        snprintf(buf, buf_len, "LD   %s,$%02X", r8[(op>>3)&7], n);
        return g_pc - addr;
    }

    /* LD r16,n16 */
    if ((op & 0xCF) == 0x01) {
        uint16_t n = fetch16();
        snprintf(buf, buf_len, "LD   %s,$%04X", r16[(op>>4)&3], n);
        return g_pc - addr;
    }

    /* INC/DEC r8 */
    if ((op & 0xC7) == 0x04) { snprintf(buf, buf_len, "INC  %s", r8[(op>>3)&7]); return g_pc - addr; }
    if ((op & 0xC7) == 0x05) { snprintf(buf, buf_len, "DEC  %s", r8[(op>>3)&7]); return g_pc - addr; }

    /* INC/DEC r16 */
    if ((op & 0xCF) == 0x03) { snprintf(buf, buf_len, "INC  %s", r16[(op>>4)&3]); return g_pc - addr; }
    if ((op & 0xCF) == 0x0B) { snprintf(buf, buf_len, "DEC  %s", r16[(op>>4)&3]); return g_pc - addr; }

    /* ADD HL,r16 */
    if ((op & 0xCF) == 0x09) { snprintf(buf, buf_len, "ADD  HL,%s", r16[(op>>4)&3]); return g_pc - addr; }

    /* PUSH/POP */
    if ((op & 0xCF) == 0xC5) { snprintf(buf, buf_len, "PUSH %s", r16af[(op>>4)&3]); return g_pc - addr; }
    if ((op & 0xCF) == 0xC1) { snprintf(buf, buf_len, "POP  %s", r16af[(op>>4)&3]); return g_pc - addr; }

    /* ALU A,n8 */
    if ((op & 0xC7) == 0xC6) {
        uint8_t n = fetch();
        snprintf(buf, buf_len, "%-5s$%02X", alu[(op>>3)&7], n);
        return g_pc - addr;
    }

    /* JR / JR cc */
    if (op == 0x18) { int8_t d = sign8(fetch()); snprintf(buf, buf_len, "JR   $%04X", (g_pc + d) & 0xFFFF); return g_pc - addr; }
    if ((op & 0xE7) == 0x20) { int8_t d = sign8(fetch()); snprintf(buf, buf_len, "JR   %s,$%04X", cc[(op>>3)&3], (g_pc + d) & 0xFFFF); return g_pc - addr; }

    /* JP / JP cc / CALL / CALL cc / RET cc / RST */
    if (op == 0xC3) { uint16_t a = fetch16(); snprintf(buf, buf_len, "JP   $%04X", a); return g_pc - addr; }
    if ((op & 0xE7) == 0xC2) { uint16_t a = fetch16(); snprintf(buf, buf_len, "JP   %s,$%04X", cc[(op>>3)&3], a); return g_pc - addr; }
    if (op == 0xCD) { uint16_t a = fetch16(); snprintf(buf, buf_len, "CALL $%04X", a); return g_pc - addr; }
    if ((op & 0xE7) == 0xC4) { uint16_t a = fetch16(); snprintf(buf, buf_len, "CALL %s,$%04X", cc[(op>>3)&3], a); return g_pc - addr; }
    if ((op & 0xE7) == 0xC0) { snprintf(buf, buf_len, "RET  %s", cc[(op>>3)&3]); return g_pc - addr; }
    if ((op & 0xC7) == 0xC7) { snprintf(buf, buf_len, "RST  $%02X", op & 0x38); return g_pc - addr; }

    /* LD (a16),A / LD A,(a16) / LD (a16),SP */
    if (op == 0xEA) { uint16_t a = fetch16(); snprintf(buf, buf_len, "LD   ($%04X),A", a); return g_pc - addr; }
    if (op == 0xFA) { uint16_t a = fetch16(); snprintf(buf, buf_len, "LD   A,($%04X)", a); return g_pc - addr; }
    if (op == 0x08) { uint16_t a = fetch16(); snprintf(buf, buf_len, "LD   ($%04X),SP", a); return g_pc - addr; }

    /* LDH */
    if (op == 0xE0) { uint8_t n = fetch(); snprintf(buf, buf_len, "LDH  ($FF%02X),A", n); return g_pc - addr; }
    if (op == 0xF0) { uint8_t n = fetch(); snprintf(buf, buf_len, "LDH  A,($FF%02X)", n); return g_pc - addr; }
    if (op == 0xE2) { snprintf(buf, buf_len, "LD   ($FF00+C),A"); return g_pc - addr; }
    if (op == 0xF2) { snprintf(buf, buf_len, "LD   A,($FF00+C)"); return g_pc - addr; }

    /* LD (r16),A / LD A,(r16) / LDI/LDD */
    if (op == 0x02) { snprintf(buf, buf_len, "LD   (BC),A"); return g_pc - addr; }
    if (op == 0x12) { snprintf(buf, buf_len, "LD   (DE),A"); return g_pc - addr; }
    if (op == 0x22) { snprintf(buf, buf_len, "LD   (HL+),A"); return g_pc - addr; }
    if (op == 0x32) { snprintf(buf, buf_len, "LD   (HL-),A"); return g_pc - addr; }
    if (op == 0x0A) { snprintf(buf, buf_len, "LD   A,(BC)"); return g_pc - addr; }
    if (op == 0x1A) { snprintf(buf, buf_len, "LD   A,(DE)"); return g_pc - addr; }
    if (op == 0x2A) { snprintf(buf, buf_len, "LD   A,(HL+)"); return g_pc - addr; }
    if (op == 0x3A) { snprintf(buf, buf_len, "LD   A,(HL-)"); return g_pc - addr; }

    /* ADD SP,e8 / LD HL,SP+e8 */
    if (op == 0xE8) { int8_t d = sign8(fetch()); snprintf(buf, buf_len, "ADD  SP,%d", d); return g_pc - addr; }
    if (op == 0xF8) { int8_t d = sign8(fetch()); snprintf(buf, buf_len, "LD   HL,SP+%d", d); return g_pc - addr; }

    /* CB prefix */
    if (op == 0xCB) {
        uint8_t cb = fetch();
        static const char *cbops[] = {"RLC","RRC","RL","RR","SLA","SRA","SWAP","SRL"};
        if (cb < 0x40) { snprintf(buf, buf_len, "%-5s%s", cbops[(cb>>3)&7], r8[cb&7]); }
        else if (cb < 0x80) { snprintf(buf, buf_len, "BIT  %d,%s", (cb>>3)&7, r8[cb&7]); }
        else if (cb < 0xC0) { snprintf(buf, buf_len, "RES  %d,%s", (cb>>3)&7, r8[cb&7]); }
        else { snprintf(buf, buf_len, "SET  %d,%s", (cb>>3)&7, r8[cb&7]); }
        return g_pc - addr;
    }

    snprintf(buf, buf_len, "DB   $%02X", op);
    return g_pc - addr;
}
