/*
 * m68k_disasm.c — Compact Motorola 68000 disassembler
 *
 * Covers the most common 68000 instructions for debugging.
 * Unknown opcodes shown as "dc.w $XXXX".
 */

#include "m68k_disasm.h"
#include <stdio.h>
#include <string.h>

static DisasmReadWord g_read;
static uint32_t g_pc;

static uint16_t fetch(void) {
    uint16_t w = g_read(g_pc);
    g_pc += 2;
    return w;
}

static const char *dreg(int n) {
    static const char *r[] = {"d0","d1","d2","d3","d4","d5","d6","d7"};
    return r[n & 7];
}

static const char *areg(int n) {
    static const char *r[] = {"a0","a1","a2","a3","a4","a5","a6","a7"};
    return r[n & 7];
}

static const char *cc_names[] = {
    "t","f","hi","ls","cc","cs","ne","eq",
    "vc","vs","pl","mi","ge","lt","gt","le"
};

/* Decode effective address, append to buf. Returns chars written. */
static int decode_ea(char *buf, int max, int mode, int reg, int size) {
    int n = 0;
    switch (mode) {
    case 0: n = snprintf(buf, max, "%s", dreg(reg)); break;
    case 1: n = snprintf(buf, max, "%s", areg(reg)); break;
    case 2: n = snprintf(buf, max, "(%s)", areg(reg)); break;
    case 3: n = snprintf(buf, max, "(%s)+", areg(reg)); break;
    case 4: n = snprintf(buf, max, "-(%s)", areg(reg)); break;
    case 5: { int16_t d = (int16_t)fetch(); n = snprintf(buf, max, "%d(%s)", d, areg(reg)); break; }
    case 6: { uint16_t ext = fetch();
              int8_t d8 = (int8_t)(ext & 0xFF);
              int xr = (ext >> 12) & 7;
              const char *xn = (ext & 0x8000) ? areg(xr) : dreg(xr);
              n = snprintf(buf, max, "%d(%s,%s)", d8, areg(reg), xn); break; }
    case 7:
        switch (reg) {
        case 0: { uint16_t w = fetch(); n = snprintf(buf, max, "$%X.w", w); break; }
        case 1: { uint16_t hi = fetch(); uint16_t lo = fetch();
                   uint32_t a = ((uint32_t)hi << 16) | lo;
                   n = snprintf(buf, max, "$%X.l", a); break; }
        case 2: { int16_t d = (int16_t)fetch(); n = snprintf(buf, max, "%d(pc)", d); break; }
        case 3: { uint16_t ext = fetch();
                   int8_t d8 = (int8_t)(ext & 0xFF);
                   int xr = (ext >> 12) & 7;
                   const char *xn = (ext & 0x8000) ? areg(xr) : dreg(xr);
                   n = snprintf(buf, max, "%d(pc,%s)", d8, xn); break; }
        case 4:
            if (size == 1) { uint16_t w = fetch(); n = snprintf(buf, max, "#$%X", w); }
            else if (size == 2) { uint16_t hi = fetch(); uint16_t lo = fetch();
                                  n = snprintf(buf, max, "#$%X", ((uint32_t)hi << 16) | lo); }
            else { uint16_t w = fetch(); n = snprintf(buf, max, "#$%X", w & 0xFF); }
            break;
        default: n = snprintf(buf, max, "???"); break;
        }
        break;
    default: n = snprintf(buf, max, "???"); break;
    }
    return n;
}

static const char *size_suffix(int sz) {
    if (sz == 0) return ".b";
    if (sz == 1) return ".w";
    return ".l";
}

int m68k_disasm(uint32_t addr, char *buf, int buf_len, DisasmReadWord read_word) {
    g_read = read_word;
    g_pc = addr;
    buf[0] = 0;

    uint16_t op = fetch();
    int top4 = (op >> 12) & 0xF;

    switch (top4) {
    case 0x0: {
        /* Immediate operations / bit operations */
        if ((op & 0xFF00) == 0x0000 && (op & 0xC0) != 0xC0) {
            /* ORI */
            int sz = (op >> 6) & 3;
            char ea[32]; uint16_t imm;
            if (sz == 2) { uint16_t hi = fetch(); uint16_t lo = fetch(); imm = 0; snprintf(buf, buf_len, "ori.l   #$%X,", ((uint32_t)hi << 16) | lo); }
            else { imm = fetch(); snprintf(buf, buf_len, "ori%s  #$%X,", size_suffix(sz), sz == 0 ? imm & 0xFF : imm); }
            int n = (int)strlen(buf);
            decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
        } else if ((op & 0xFF00) == 0x0200 && (op & 0xC0) != 0xC0) {
            int sz = (op >> 6) & 3;
            uint16_t imm = fetch();
            int n = snprintf(buf, buf_len, "andi%s #$%X,", size_suffix(sz), sz == 0 ? imm & 0xFF : imm);
            decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
        } else if ((op & 0xFF00) == 0x0400 && (op & 0xC0) != 0xC0) {
            int sz = (op >> 6) & 3;
            uint16_t imm = fetch();
            int n = snprintf(buf, buf_len, "subi%s #$%X,", size_suffix(sz), sz == 0 ? imm & 0xFF : imm);
            decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
        } else if ((op & 0xFF00) == 0x0600 && (op & 0xC0) != 0xC0) {
            int sz = (op >> 6) & 3;
            uint16_t imm = fetch();
            int n = snprintf(buf, buf_len, "addi%s #$%X,", size_suffix(sz), sz == 0 ? imm & 0xFF : imm);
            decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
        } else if ((op & 0xFF00) == 0x0A00 && (op & 0xC0) != 0xC0) {
            int sz = (op >> 6) & 3;
            uint16_t imm = fetch();
            int n = snprintf(buf, buf_len, "eori%s #$%X,", size_suffix(sz), sz == 0 ? imm & 0xFF : imm);
            decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
        } else if ((op & 0xFF00) == 0x0C00 && (op & 0xC0) != 0xC0) {
            int sz = (op >> 6) & 3;
            uint16_t imm = fetch();
            int n = snprintf(buf, buf_len, "cmpi%s #$%X,", size_suffix(sz), sz == 0 ? imm & 0xFF : imm);
            decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
        } else if ((op & 0xF138) == 0x0108) {
            /* MOVEP */
            snprintf(buf, buf_len, "movep   (unimpl)");
        } else if ((op & 0xFFC0) == 0x0800) {
            uint16_t bit = fetch() & 31;
            int n = snprintf(buf, buf_len, "btst    #%d,", bit);
            decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, 0);
        } else {
            snprintf(buf, buf_len, "dc.w    $%04X", op);
        }
        break;
    }

    case 0x1: case 0x2: case 0x3: {
        /* MOVE / MOVEA */
        int sz = (top4 == 1) ? 0 : (top4 == 3) ? 1 : 2;
        int dst_reg = (op >> 9) & 7;
        int dst_mode = (op >> 6) & 7;
        int src_mode = (op >> 3) & 7;
        int src_reg = op & 7;
        if (dst_mode == 1) {
            int n = snprintf(buf, buf_len, "movea%s ", size_suffix(sz));
            n += decode_ea(buf + n, buf_len - n, src_mode, src_reg, sz);
            snprintf(buf + n, buf_len - n, ",%s", areg(dst_reg));
        } else {
            int n = snprintf(buf, buf_len, "move%s  ", size_suffix(sz));
            n += decode_ea(buf + n, buf_len - n, src_mode, src_reg, sz);
            n += snprintf(buf + n, buf_len - n, ",");
            decode_ea(buf + n, buf_len - n, dst_mode, dst_reg, sz);
        }
        break;
    }

    case 0x4: {
        /* Miscellaneous */
        if (op == 0x4E71) snprintf(buf, buf_len, "nop");
        else if (op == 0x4E75) snprintf(buf, buf_len, "rts");
        else if (op == 0x4E73) snprintf(buf, buf_len, "rte");
        else if (op == 0x4E76) snprintf(buf, buf_len, "trapv");
        else if (op == 0x4E77) snprintf(buf, buf_len, "rtr");
        else if ((op & 0xFFF0) == 0x4E40) snprintf(buf, buf_len, "trap    #%d", op & 0xF);
        else if ((op & 0xFFF8) == 0x4E50) {
            int16_t d = (int16_t)fetch();
            snprintf(buf, buf_len, "link    %s,#%d", areg(op & 7), d);
        }
        else if ((op & 0xFFF8) == 0x4E58) snprintf(buf, buf_len, "unlk    %s", areg(op & 7));
        else if ((op & 0xFFF8) == 0x4E60) snprintf(buf, buf_len, "move.l  %s,usp", areg(op & 7));
        else if ((op & 0xFFF8) == 0x4E68) snprintf(buf, buf_len, "move.l  usp,%s", areg(op & 7));
        else if ((op & 0xFFC0) == 0x4EC0) {
            int n = snprintf(buf, buf_len, "jmp     ");
            decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, 1);
        }
        else if ((op & 0xFFC0) == 0x4E80) {
            int n = snprintf(buf, buf_len, "jsr     ");
            decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, 1);
        }
        else if ((op & 0xFFC0) == 0x41C0) {
            int n = snprintf(buf, buf_len, "lea     ");
            n += decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, 2);
            snprintf(buf + n, buf_len - n, ",%s", areg((op >> 9) & 7));
        }
        else if ((op & 0xFFC0) == 0x4840) {
            int n = snprintf(buf, buf_len, "pea     ");
            decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, 2);
        }
        else if ((op & 0xFF00) == 0x4200) {
            int sz = (op >> 6) & 3;
            int n = snprintf(buf, buf_len, "clr%s   ", size_suffix(sz));
            decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
        }
        else if ((op & 0xFF00) == 0x4400) {
            int sz = (op >> 6) & 3;
            int n = snprintf(buf, buf_len, "neg%s   ", size_suffix(sz));
            decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
        }
        else if ((op & 0xFF00) == 0x4600) {
            int sz = (op >> 6) & 3;
            int n = snprintf(buf, buf_len, "not%s   ", size_suffix(sz));
            decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
        }
        else if ((op & 0xFF00) == 0x4A00) {
            int sz = (op >> 6) & 3;
            int n = snprintf(buf, buf_len, "tst%s   ", size_suffix(sz));
            decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
        }
        else if ((op & 0xFFF8) == 0x4840) {
            snprintf(buf, buf_len, "swap    %s", dreg(op & 7));
        }
        else if ((op & 0xFFF8) == 0x4880) snprintf(buf, buf_len, "ext.w   %s", dreg(op & 7));
        else if ((op & 0xFFF8) == 0x48C0) snprintf(buf, buf_len, "ext.l   %s", dreg(op & 7));
        else if ((op & 0xFB80) == 0x4880) {
            /* MOVEM */
            uint16_t mask = fetch();
            int n = snprintf(buf, buf_len, "movem%s ", (op & 0x40) ? ".l" : ".w");
            if (op & 0x0400) { /* memory to register */
                n += decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, 1);
                snprintf(buf + n, buf_len - n, ",#$%04X", mask);
            } else {
                n += snprintf(buf + n, buf_len - n, "#$%04X,", mask);
                decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, 1);
            }
        }
        else snprintf(buf, buf_len, "dc.w    $%04X", op);
        break;
    }

    case 0x5: {
        if ((op & 0xF0C0) == 0x50C0 && (op & 0x38) == 0x08) {
            /* DBcc */
            int cc = (op >> 8) & 0xF;
            int16_t d = (int16_t)fetch();
            uint32_t target = g_pc - 2 + d;
            snprintf(buf, buf_len, "db%s    %s,$%X", cc_names[cc], dreg(op & 7), target);
        } else if ((op & 0xC0) != 0xC0) {
            /* ADDQ / SUBQ */
            int sz = (op >> 6) & 3;
            int data = (op >> 9) & 7; if (data == 0) data = 8;
            const char *mn = (op & 0x100) ? "subq" : "addq";
            int n = snprintf(buf, buf_len, "%s%s  #%d,", mn, size_suffix(sz), data);
            decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
        } else {
            /* Scc */
            int cc = (op >> 8) & 0xF;
            int n = snprintf(buf, buf_len, "s%s     ", cc_names[cc]);
            decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, 0);
        }
        break;
    }

    case 0x6: {
        /* Bcc / BRA / BSR */
        int cc = (op >> 8) & 0xF;
        int8_t d8 = (int8_t)(op & 0xFF);
        int32_t disp;
        if (d8 == 0) { disp = (int16_t)fetch(); }
        else { disp = d8; }
        uint32_t target = addr + 2 + disp;
        if (cc == 0) snprintf(buf, buf_len, "bra     $%X", target);
        else if (cc == 1) snprintf(buf, buf_len, "bsr     $%X", target);
        else snprintf(buf, buf_len, "b%s     $%X", cc_names[cc], target);
        break;
    }

    case 0x7: {
        /* MOVEQ */
        int8_t data = (int8_t)(op & 0xFF);
        snprintf(buf, buf_len, "moveq   #%d,%s", data, dreg((op >> 9) & 7));
        break;
    }

    case 0x8: {
        if ((op & 0x1F0) == 0x100) {
            /* SBCD */
            snprintf(buf, buf_len, "sbcd    (unimpl)");
        } else if ((op & 0xC0) == 0xC0) {
            /* DIVU/DIVS */
            const char *mn = (op & 0x100) ? "divs" : "divu";
            int n = snprintf(buf, buf_len, "%s.w  ", mn);
            n += decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, 1);
            snprintf(buf + n, buf_len - n, ",%s", dreg((op >> 9) & 7));
        } else {
            int sz = (op >> 6) & 3;
            int n = snprintf(buf, buf_len, "or%s    ", size_suffix(sz));
            if (op & 0x100) {
                n += snprintf(buf + n, buf_len - n, "%s,", dreg((op >> 9) & 7));
                decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
            } else {
                n += decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
                snprintf(buf + n, buf_len - n, ",%s", dreg((op >> 9) & 7));
            }
        }
        break;
    }

    case 0x9: case 0xD: {
        /* SUB/ADD */
        const char *mn = (top4 == 0x9) ? "sub" : "add";
        int sz = (op >> 6) & 3;
        if ((op & 0xC0) == 0xC0) {
            /* SUBA/ADDA */
            sz = (op & 0x100) ? 2 : 1;
            int n = snprintf(buf, buf_len, "%sa%s  ", mn, size_suffix(sz));
            n += decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
            snprintf(buf + n, buf_len - n, ",%s", areg((op >> 9) & 7));
        } else {
            int n = snprintf(buf, buf_len, "%s%s   ", mn, size_suffix(sz));
            if (op & 0x100) {
                n += snprintf(buf + n, buf_len - n, "%s,", dreg((op >> 9) & 7));
                decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
            } else {
                n += decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
                snprintf(buf + n, buf_len - n, ",%s", dreg((op >> 9) & 7));
            }
        }
        break;
    }

    case 0xB: {
        /* CMP / CMPA / EOR */
        int sz = (op >> 6) & 3;
        if ((op & 0xC0) == 0xC0) {
            sz = (op & 0x100) ? 2 : 1;
            int n = snprintf(buf, buf_len, "cmpa%s  ", size_suffix(sz));
            n += decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
            snprintf(buf + n, buf_len - n, ",%s", areg((op >> 9) & 7));
        } else if (op & 0x100) {
            int n = snprintf(buf, buf_len, "eor%s   %s,", size_suffix(sz), dreg((op >> 9) & 7));
            decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
        } else {
            int n = snprintf(buf, buf_len, "cmp%s   ", size_suffix(sz));
            n += decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
            snprintf(buf + n, buf_len - n, ",%s", dreg((op >> 9) & 7));
        }
        break;
    }

    case 0xC: {
        if ((op & 0xC0) == 0xC0) {
            const char *mn = (op & 0x100) ? "muls" : "mulu";
            int n = snprintf(buf, buf_len, "%s.w  ", mn);
            n += decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, 1);
            snprintf(buf + n, buf_len - n, ",%s", dreg((op >> 9) & 7));
        } else if ((op & 0x1F8) == 0x140 || (op & 0x1F8) == 0x148 || (op & 0x1F8) == 0x188) {
            /* EXG */
            snprintf(buf, buf_len, "exg     (unimpl)");
        } else {
            int sz = (op >> 6) & 3;
            int n = snprintf(buf, buf_len, "and%s   ", size_suffix(sz));
            if (op & 0x100) {
                n += snprintf(buf + n, buf_len - n, "%s,", dreg((op >> 9) & 7));
                decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
            } else {
                n += decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, sz);
                snprintf(buf + n, buf_len - n, ",%s", dreg((op >> 9) & 7));
            }
        }
        break;
    }

    case 0xE: {
        /* Shift / Rotate */
        const char *ops[] = {"as","ls","rox","ro"};
        int type = (op >> 3) & 3;
        const char *dir = (op & 0x100) ? "l" : "r";
        if ((op & 0xC0) == 0xC0) {
            /* Memory shift (word only) */
            int n = snprintf(buf, buf_len, "%s%s.w  ", ops[type], dir);
            decode_ea(buf + n, buf_len - n, (op >> 3) & 7, op & 7, 1);
        } else {
            int sz = (op >> 6) & 3;
            int cnt_reg = (op >> 9) & 7;
            if (op & 0x20) {
                snprintf(buf, buf_len, "%s%s%s   %s,%s", ops[type], dir, size_suffix(sz),
                         dreg(cnt_reg), dreg(op & 7));
            } else {
                int cnt = cnt_reg; if (cnt == 0) cnt = 8;
                snprintf(buf, buf_len, "%s%s%s   #%d,%s", ops[type], dir, size_suffix(sz),
                         cnt, dreg(op & 7));
            }
        }
        break;
    }

    default:
        snprintf(buf, buf_len, "dc.w    $%04X", op);
        break;
    }

    return (int)(g_pc - addr);
}
