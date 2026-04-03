/*
 * m6809_disasm.c — Compact Motorola 6809 disassembler
 *
 * Covers the most common instructions for debugging.
 */

#include "m6809_disasm.h"
#include <stdio.h>
#include <string.h>

static Disasm6809Read g_read;
static unsigned g_pc;

static unsigned char fetch8(void) { return g_read(g_pc++); }
static unsigned fetch16(void) { unsigned h = fetch8(); return (h << 8) | fetch8(); }
static int sign8(unsigned char b) { return (b & 0x80) ? (int)b - 256 : (int)b; }
static int sign16(unsigned w) { return (w & 0x8000) ? (int)w - 65536 : (int)w; }

static const char *idx_reg[] = {"X", "Y", "U", "S"};

static int decode_indexed(char *buf, int max) {
    unsigned char pb = fetch8();
    int reg = (pb >> 5) & 3;
    const char *r = idx_reg[reg];

    if (!(pb & 0x80)) {
        /* 5-bit offset */
        int off = (pb & 0x1F);
        if (off & 0x10) off -= 32;
        return snprintf(buf, max, "%d,%s", off, r);
    }

    int indirect = pb & 0x10;
    switch (pb & 0x0F) {
    case 0x00: return snprintf(buf, max, ",%s+", r);
    case 0x01: return snprintf(buf, max, ",%s++", r);
    case 0x02: return snprintf(buf, max, ",-%s", r);
    case 0x03: return snprintf(buf, max, ",--%s", r);
    case 0x04: return snprintf(buf, max, ",%s", r);
    case 0x05: return snprintf(buf, max, "B,%s", r);
    case 0x06: return snprintf(buf, max, "A,%s", r);
    case 0x08: { int o = sign8(fetch8()); return snprintf(buf, max, "%d,%s", o, r); }
    case 0x09: { int o = sign16(fetch16()); return snprintf(buf, max, "%d,%s", o, r); }
    case 0x0B: return snprintf(buf, max, "D,%s", r);
    case 0x0C: { int o = sign8(fetch8()); return snprintf(buf, max, "%d,PCR", o); }
    case 0x0D: { int o = sign16(fetch16()); return snprintf(buf, max, "%d,PCR", o); }
    case 0x0F: if (indirect) { unsigned a = fetch16(); return snprintf(buf, max, "[$%04X]", a); }
    default: return snprintf(buf, max, "???");
    }
}

/* Main opcode tables (simplified) */
static const char *inh_ops[256]; /* inherent/implied opcodes */
static int tables_init = 0;

static void init_tables(void) {
    if (tables_init) return;
    tables_init = 1;
    memset(inh_ops, 0, sizeof(inh_ops));
    inh_ops[0x12] = "NOP"; inh_ops[0x13] = "SYNC"; inh_ops[0x19] = "DAA";
    inh_ops[0x1D] = "SEX"; inh_ops[0x39] = "RTS"; inh_ops[0x3A] = "ABX";
    inh_ops[0x3B] = "RTI"; inh_ops[0x3D] = "MUL"; inh_ops[0x3F] = "SWI";
    inh_ops[0x40] = "NEGA"; inh_ops[0x43] = "COMA"; inh_ops[0x44] = "LSRA";
    inh_ops[0x46] = "RORA"; inh_ops[0x47] = "ASRA"; inh_ops[0x48] = "ASLA";
    inh_ops[0x49] = "ROLA"; inh_ops[0x4A] = "DECA"; inh_ops[0x4C] = "INCA";
    inh_ops[0x4D] = "TSTA"; inh_ops[0x4F] = "CLRA";
    inh_ops[0x50] = "NEGB"; inh_ops[0x53] = "COMB"; inh_ops[0x54] = "LSRB";
    inh_ops[0x56] = "RORB"; inh_ops[0x57] = "ASRB"; inh_ops[0x58] = "ASLB";
    inh_ops[0x59] = "ROLB"; inh_ops[0x5A] = "DECB"; inh_ops[0x5C] = "INCB";
    inh_ops[0x5D] = "TSTB"; inh_ops[0x5F] = "CLRB";
}

int m6809_disasm(unsigned addr, char *buf, int buf_len, Disasm6809Read read) {
    init_tables();
    g_read = read;
    g_pc = addr;
    buf[0] = 0;

    unsigned char op = fetch8();

    /* Inherent */
    if (inh_ops[op]) { snprintf(buf, buf_len, "%s", inh_ops[op]); return g_pc - addr; }

    /* Branches (8-bit offset) */
    if (op >= 0x20 && op <= 0x2F) {
        static const char *br[] = {"BRA","BRN","BHI","BLS","BCC","BCS","BNE","BEQ",
                                    "BVC","BVS","BPL","BMI","BGE","BLT","BGT","BLE"};
        int off = sign8(fetch8());
        snprintf(buf, buf_len, "%-6s $%04X", br[op - 0x20], (g_pc + off) & 0xFFFF);
        return g_pc - addr;
    }

    /* Long branches (16-bit offset) */
    if (op == 0x16) { int off = sign16(fetch16()); snprintf(buf, buf_len, "LBRA   $%04X", (g_pc + off) & 0xFFFF); return g_pc - addr; }
    if (op == 0x17) { int off = sign16(fetch16()); snprintf(buf, buf_len, "LBSR   $%04X", (g_pc + off) & 0xFFFF); return g_pc - addr; }
    if (op == 0x8D) { int off = sign8(fetch8()); snprintf(buf, buf_len, "BSR    $%04X", (g_pc + off) & 0xFFFF); return g_pc - addr; }

    /* LEA */
    if (op == 0x30) { char idx[32]; decode_indexed(idx, sizeof(idx)); snprintf(buf, buf_len, "LEAX   %s", idx); return g_pc - addr; }
    if (op == 0x31) { char idx[32]; decode_indexed(idx, sizeof(idx)); snprintf(buf, buf_len, "LEAY   %s", idx); return g_pc - addr; }
    if (op == 0x32) { char idx[32]; decode_indexed(idx, sizeof(idx)); snprintf(buf, buf_len, "LEAS   %s", idx); return g_pc - addr; }
    if (op == 0x33) { char idx[32]; decode_indexed(idx, sizeof(idx)); snprintf(buf, buf_len, "LEAU   %s", idx); return g_pc - addr; }

    /* PSHS/PULS/PSHU/PULU */
    if (op == 0x34) { snprintf(buf, buf_len, "PSHS   #$%02X", fetch8()); return g_pc - addr; }
    if (op == 0x35) { snprintf(buf, buf_len, "PULS   #$%02X", fetch8()); return g_pc - addr; }
    if (op == 0x36) { snprintf(buf, buf_len, "PSHU   #$%02X", fetch8()); return g_pc - addr; }
    if (op == 0x37) { snprintf(buf, buf_len, "PULU   #$%02X", fetch8()); return g_pc - addr; }

    /* EXG/TFR */
    if (op == 0x1E) { snprintf(buf, buf_len, "EXG    #$%02X", fetch8()); return g_pc - addr; }
    if (op == 0x1F) { snprintf(buf, buf_len, "TFR    #$%02X", fetch8()); return g_pc - addr; }

    /* ANDCC/ORCC/CWAI */
    if (op == 0x1A) { snprintf(buf, buf_len, "ORCC   #$%02X", fetch8()); return g_pc - addr; }
    if (op == 0x1C) { snprintf(buf, buf_len, "ANDCC  #$%02X", fetch8()); return g_pc - addr; }
    if (op == 0x3C) { snprintf(buf, buf_len, "CWAI   #$%02X", fetch8()); return g_pc - addr; }

    /* JMP/JSR */
    if (op == 0x7E) { unsigned a = fetch16(); snprintf(buf, buf_len, "JMP    $%04X", a); return g_pc - addr; }
    if (op == 0xBD) { unsigned a = fetch16(); snprintf(buf, buf_len, "JSR    $%04X", a); return g_pc - addr; }
    if (op == 0xAD) { char idx[32]; decode_indexed(idx, sizeof(idx)); snprintf(buf, buf_len, "JSR    %s", idx); return g_pc - addr; }
    if (op == 0x6E) { char idx[32]; decode_indexed(idx, sizeof(idx)); snprintf(buf, buf_len, "JMP    %s", idx); return g_pc - addr; }
    if (op == 0x9D) { snprintf(buf, buf_len, "JSR    <$%02X", fetch8()); return g_pc - addr; }

    /* Common load/store/alu patterns: immediate, direct, indexed, extended */
    /* Format: opcode base at 0x80/0x90/0xA0/0xB0 etc. */
    {
        static const char *alu8[] = {"SUB","CMP","SBC","???","AND","BIT","LD","ST","EOR","ADC","OR","ADD"};
        static const char *alu8_b[] = {"SUB","CMP","SBC","ADD","AND","BIT","LD","ST","EOR","ADC","OR","ADD"};
        int base = -1, mode = -1;
        const char *reg = "A";
        const char **tbl = alu8;

        if (op >= 0x80 && op <= 0x8B) { base = op - 0x80; mode = 0; } /* imm A */
        if (op >= 0x90 && op <= 0x9B) { base = op - 0x90; mode = 1; } /* dir A */
        if (op >= 0xA0 && op <= 0xAB) { base = op - 0xA0; mode = 2; } /* idx A */
        if (op >= 0xB0 && op <= 0xBB) { base = op - 0xB0; mode = 3; } /* ext A */
        if (op >= 0xC0 && op <= 0xCB) { base = op - 0xC0; mode = 0; reg = "B"; tbl = alu8_b; }
        if (op >= 0xD0 && op <= 0xDB) { base = op - 0xD0; mode = 1; reg = "B"; tbl = alu8_b; }
        if (op >= 0xE0 && op <= 0xEB) { base = op - 0xE0; mode = 2; reg = "B"; tbl = alu8_b; }
        if (op >= 0xF0 && op <= 0xFB) { base = op - 0xF0; mode = 3; reg = "B"; tbl = alu8_b; }

        if (base >= 0 && base < 12) {
            char mn[8]; snprintf(mn, sizeof(mn), "%s%s", tbl[base], reg);
            switch (mode) {
            case 0: snprintf(buf, buf_len, "%-6s #$%02X", mn, fetch8()); break;
            case 1: snprintf(buf, buf_len, "%-6s <$%02X", mn, fetch8()); break;
            case 2: { char idx[32]; decode_indexed(idx, sizeof(idx)); snprintf(buf, buf_len, "%-6s %s", mn, idx); break; }
            case 3: snprintf(buf, buf_len, "%-6s $%04X", mn, fetch16()); break;
            }
            return g_pc - addr;
        }
    }

    /* 16-bit LD/ST/CMP/ADD for X,D,U,S */
    {
        struct { unsigned char op; const char *mn; int mode; } ops16[] = {
            {0x8E, "LDX", 0}, {0x9E, "LDX", 1}, {0xAE, "LDX", 2}, {0xBE, "LDX", 3},
            {0x8C, "CMPX",0}, {0x9C, "CMPX",1}, {0xAC, "CMPX",2}, {0xBC, "CMPX",3},
            {0xCC, "LDD", 0}, {0xDC, "LDD", 1}, {0xEC, "LDD", 2}, {0xFC, "LDD", 3},
            {0xCE, "LDU", 0}, {0xDE, "LDU", 1}, {0xEE, "LDU", 2}, {0xFE, "LDU", 3},
            {0x8F, "STX", 1}, {0x9F, "STX", 1}, {0xAF, "STX", 2}, {0xBF, "STX", 3},
            {0xCD, "STD", 1}, {0xDD, "STD", 1}, {0xED, "STD", 2}, {0xFD, "STD", 3},
            {0xCF, "STU", 1}, {0xDF, "STU", 1}, {0xEF, "STU", 2}, {0xFF, "STU", 3},
            {0xC3, "ADDD",0}, {0xD3, "ADDD",1}, {0xE3, "ADDD",2}, {0xF3, "ADDD",3},
            {0x83, "SUBD",0}, {0x93, "SUBD",1}, {0xA3, "SUBD",2}, {0xB3, "SUBD",3},
        };
        for (int i = 0; i < (int)(sizeof(ops16)/sizeof(ops16[0])); i++) {
            if (op == ops16[i].op) {
                switch (ops16[i].mode) {
                case 0: snprintf(buf, buf_len, "%-6s #$%04X", ops16[i].mn, fetch16()); break;
                case 1: snprintf(buf, buf_len, "%-6s <$%02X", ops16[i].mn, fetch8()); break;
                case 2: { char idx[32]; decode_indexed(idx, sizeof(idx)); snprintf(buf, buf_len, "%-6s %s", ops16[i].mn, idx); break; }
                case 3: snprintf(buf, buf_len, "%-6s $%04X", ops16[i].mn, fetch16()); break;
                }
                return g_pc - addr;
            }
        }
    }

    /* NEG/COM/LSR/ROR/ASR/ASL/ROL/DEC/INC/TST/CLR (memory) */
    {
        static const char *mem_ops[] = {"NEG","???","???","COM","LSR","???","ROR","ASR","ASL","ROL","DEC","???","INC","TST","JMP","CLR"};
        int sub = op & 0x0F;
        if (op >= 0x00 && op <= 0x0F) { snprintf(buf, buf_len, "%-6s <$%02X", mem_ops[sub], fetch8()); return g_pc - addr; }
        if (op >= 0x60 && op <= 0x6F && sub != 0x0E) { char idx[32]; decode_indexed(idx, sizeof(idx)); snprintf(buf, buf_len, "%-6s %s", mem_ops[sub], idx); return g_pc - addr; }
        if (op >= 0x70 && op <= 0x7F && sub != 0x0E) { snprintf(buf, buf_len, "%-6s $%04X", mem_ops[sub], fetch16()); return g_pc - addr; }
    }

    /* Page 2 (0x10 prefix) */
    if (op == 0x10) {
        unsigned char op2 = fetch8();
        if (op2 >= 0x20 && op2 <= 0x2F) {
            static const char *lbr[] = {"LBRA","LBRN","LBHI","LBLS","LBCC","LBCS","LBNE","LBEQ",
                                        "LBVC","LBVS","LBPL","LBMI","LBGE","LBLT","LBGT","LBLE"};
            int off = sign16(fetch16());
            snprintf(buf, buf_len, "%-6s $%04X", lbr[op2 - 0x20], (g_pc + off) & 0xFFFF);
            return g_pc - addr;
        }
        if (op2 == 0x3F) { snprintf(buf, buf_len, "SWI2"); return g_pc - addr; }
        if (op2 == 0x8E) { snprintf(buf, buf_len, "LDY    #$%04X", fetch16()); return g_pc - addr; }
        if (op2 == 0xCE) { snprintf(buf, buf_len, "LDS    #$%04X", fetch16()); return g_pc - addr; }
        snprintf(buf, buf_len, "FCB    $10,$%02X", op2);
        return g_pc - addr;
    }

    /* Page 3 (0x11 prefix) */
    if (op == 0x11) {
        unsigned char op2 = fetch8();
        if (op2 == 0x3F) { snprintf(buf, buf_len, "SWI3"); return g_pc - addr; }
        snprintf(buf, buf_len, "FCB    $11,$%02X", op2);
        return g_pc - addr;
    }

    snprintf(buf, buf_len, "FCB    $%02X", op);
    return g_pc - addr;
}
