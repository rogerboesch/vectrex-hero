/*
 * tos2ql.c — Convert Atari ST TOS executable to self-relocating QDOS binary.
 *
 * TOS format:
 *   28-byte header: magic, text_size, data_size, bss_size, sym_size, ...
 *   text section (code)
 *   data section
 *   symbol table (skip)
 *   relocation table: first longword = offset of first fixup,
 *     then bytes: 0=end, 1=add 254, 2-255=delta to next fixup
 *
 * Output QDOS format:
 *   Self-relocating stub (36 bytes)
 *   Relocation table (compact: 16-bit deltas, 0=end)
 *   Code + data
 *   XTcc trailer (8 bytes)
 *
 * Usage: tos2ql input.tos output_ql
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Read big-endian values from buffer */
static uint16_t rd16(const uint8_t *p) { return (p[0] << 8) | p[1]; }
static uint32_t rd32(const uint8_t *p) { return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

/* Write big-endian values to buffer */
static void wr16(uint8_t *p, uint16_t v) { p[0] = v >> 8; p[1] = v & 0xFF; }
static void wr32(uint8_t *p, uint32_t v) {
    p[0] = (v >> 24) & 0xFF; p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF; p[3] = v & 0xFF;
}

#define MAX_FIXUPS 8192
#define STUB_SIZE 36

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s input.tos output_ql\n", argv[0]);
        return 1;
    }

    /* Read input file */
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "Cannot open %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc(file_size);
    fread(data, 1, file_size, f);
    fclose(f);

    /* Parse TOS header (28 bytes) */
    uint32_t text_size = rd32(data + 2);
    uint32_t data_size = rd32(data + 6);
    uint32_t bss_size  = rd32(data + 10);
    uint32_t sym_size  = rd32(data + 14);

    printf("TOS: text=%u data=%u bss=%u sym=%u\n", text_size, data_size, bss_size, sym_size);

    uint32_t text_start = 28;
    uint32_t data_start = text_start + text_size;
    uint32_t sym_start  = data_start + data_size;
    uint32_t reloc_start = sym_start + sym_size;
    uint32_t code_len = text_size + data_size;

    /* Parse relocation table */
    uint32_t fixups[MAX_FIXUPS];
    int fixup_count = 0;
    uint32_t pos = reloc_start;

    if (pos + 4 <= (uint32_t)file_size) {
        uint32_t first = rd32(data + pos);
        pos += 4;
        if (first != 0) {
            uint32_t offset = first;
            fixups[fixup_count++] = offset;
            while (pos < (uint32_t)file_size && fixup_count < MAX_FIXUPS) {
                uint8_t b = data[pos++];
                if (b == 0) break;
                else if (b == 1) offset += 254;
                else { offset += b; fixups[fixup_count++] = offset; }
            }
        }
    }

    printf("Relocations: %d fixups\n", fixup_count);

    /* Build compact relocation table (16-bit deltas) */
    int reloc_cap = (fixup_count + 2) * 4; /* worst case */
    uint8_t *reloc_table = calloc(reloc_cap, 1);
    int reloc_len = 0;
    uint32_t prev = 0;

    for (int i = 0; i < fixup_count; i++) {
        uint32_t delta = fixups[i] - prev;
        while (delta > 65534) {
            wr16(reloc_table + reloc_len, 65534); reloc_len += 2;
            wr16(reloc_table + reloc_len, 0);     reloc_len += 2; /* skip marker */
            delta -= 65534;
        }
        wr16(reloc_table + reloc_len, (uint16_t)delta); reloc_len += 2;
        prev = fixups[i];
    }
    wr16(reloc_table + reloc_len, 0); reloc_len += 2; /* end marker */
    wr16(reloc_table + reloc_len, 0); reloc_len += 2; /* alignment padding */

    /* Compute offsets */
    int reloc_offset = STUB_SIZE;
    int code_start_offset = STUB_SIZE + reloc_len;
    if (code_start_offset & 1) {
        reloc_table[reloc_len++] = 0; /* pad for alignment */
        code_start_offset++;
    }

    /* Build self-relocating 68K stub */
    uint8_t stub[STUB_SIZE];
    memset(stub, 0, STUB_SIZE);
    int sp = 0;

    /* lea code(pc), a0  -->  41FA disp16 */
    int disp_to_code = code_start_offset - (sp + 2);
    stub[sp++] = 0x41; stub[sp++] = 0xFA;
    wr16(stub + sp, (uint16_t)disp_to_code); sp += 2;

    /* lea reloc(pc), a1  -->  43FA disp16 */
    int disp_to_reloc = reloc_offset - (sp + 2);
    stub[sp++] = 0x43; stub[sp++] = 0xFA;
    wr16(stub + sp, (uint16_t)disp_to_reloc); sp += 2;

    /* move.l a0, d2  -->  2408  (save code base) */
    stub[sp++] = 0x24; stub[sp++] = 0x08;

    /* .loop: */
    int loop_offset = sp;

    /* move.w (a1)+, d0  -->  3019 */
    stub[sp++] = 0x30; stub[sp++] = 0x19;

    /* beq.s .done  -->  67 xx */
    int done_branch_pos = sp;
    stub[sp++] = 0x67; stub[sp++] = 0x00; /* placeholder */

    /* andi.l #$FFFF, d0  -->  0280 0000 FFFF */
    stub[sp++] = 0x02; stub[sp++] = 0x80;
    stub[sp++] = 0x00; stub[sp++] = 0x00;
    stub[sp++] = 0xFF; stub[sp++] = 0xFF;

    /* adda.l d0, a0  -->  D1C0 */
    stub[sp++] = 0xD1; stub[sp++] = 0xC0;

    /* add.l d2, (a0)  -->  D590 */
    stub[sp++] = 0xD5; stub[sp++] = 0x90;

    /* bra.s .loop */
    int loop_disp = loop_offset - (sp + 2);
    stub[sp++] = 0x60; stub[sp++] = (uint8_t)(loop_disp & 0xFF);

    /* .done: */
    int done_offset = sp;
    stub[done_branch_pos + 1] = (uint8_t)((done_offset - (done_branch_pos + 2)) & 0xFF);

    /* move.l d2, a0  -->  2042 */
    stub[sp++] = 0x20; stub[sp++] = 0x42;

    /* jmp (a0)  -->  4ED0 */
    stub[sp++] = 0x4E; stub[sp++] = 0xD0;

    /* Pad with nops */
    while (sp < STUB_SIZE) {
        stub[sp++] = 0x4E; stub[sp++] = 0x71; /* nop */
    }

    /* Write output */
    f = fopen(argv[2], "wb");
    if (!f) { fprintf(stderr, "Cannot create %s\n", argv[2]); free(data); free(reloc_table); return 1; }

    fwrite(stub, 1, STUB_SIZE, f);
    fwrite(reloc_table, 1, reloc_len, f);
    fwrite(data + text_start, 1, code_len, f);

    /* XTcc trailer (8 bytes) — marks file as executable */
    uint8_t xtcc[8];
    memcpy(xtcc, "XTcc", 4);
    wr32(xtcc + 4, bss_size + 4096);
    fwrite(xtcc, 1, 8, f);

    long total = STUB_SIZE + reloc_len + code_len + 8;
    fclose(f);

    printf("Output: %s (%ld bytes)\n", argv[2], total);
    printf("  Stub: %d, Reloc: %d, Code: %u\n", STUB_SIZE, reloc_len, code_len);
    printf("  Header: XTcc trailer (%u data space)\n", bss_size + 4096);

    free(data);
    free(reloc_table);
    return 0;
}
