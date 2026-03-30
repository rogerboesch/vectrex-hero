#!/usr/bin/env python3
"""
tos2ql.py — Convert Atari ST TOS executable to self-relocating QDOS binary.

TOS format:
  28-byte header: magic, text_size, data_size, bss_size, sym_size, ...
  text section (code)
  data section
  symbol table (skip)
  relocation table: first longword = offset of first fixup,
    then bytes: 0=end, 1=add 254, 2-255=delta to next fixup

Output QDOS format:
  Q-emulator header (30 bytes)
  Self-relocating stub (moves code, applies fixups, calls main)
  Code + data
  Relocation table (compact: list of 16-bit deltas, 0=end)
"""
import struct
import sys

def read_tos(filename):
    with open(filename, 'rb') as f:
        data = f.read()

    # TOS header (28 bytes)
    magic = struct.unpack_from('>H', data, 0)[0]
    text_size = struct.unpack_from('>I', data, 2)[0]
    data_size = struct.unpack_from('>I', data, 6)[0]
    bss_size = struct.unpack_from('>I', data, 10)[0]
    sym_size = struct.unpack_from('>I', data, 14)[0]

    print(f"TOS: text={text_size} data={data_size} bss={bss_size} sym={sym_size}")

    text_start = 28
    data_start = text_start + text_size
    sym_start = data_start + data_size
    reloc_start = sym_start + sym_size

    code_data = data[text_start:data_start + data_size]

    # Parse relocation table
    fixups = []
    pos = reloc_start
    if pos < len(data):
        first = struct.unpack_from('>I', data, pos)[0]
        pos += 4
        if first != 0:
            offset = first
            fixups.append(offset)
            while pos < len(data):
                b = data[pos]
                pos += 1
                if b == 0:
                    break
                elif b == 1:
                    offset += 254
                else:
                    offset += b
                    fixups.append(offset)

    print(f"Relocations: {len(fixups)} fixups")
    return code_data, fixups, bss_size


def make_qdos_binary(code_data, fixups, bss_size, outfile):
    # Self-relocating stub in 68K machine code
    # This runs at offset 0 of the loaded job.
    # It computes load_base, walks the relocation table, patches addresses,
    # then jumps to the original entry point.
    #
    # Register usage:
    #   a0 = current fixup pointer in code
    #   a1 = relocation table pointer
    #   a2 = load base (actual address of code start)
    #   d0 = current offset
    #   d1 = delta value

    # The stub needs to:
    # 1. lea stub_start(pc), a2  -> a2 = actual runtime address of stub
    # 2. lea code_start(pc), a3  -> a3 = actual runtime address of code
    # 3. For each fixup: read 32-bit value at code+offset, add a3, write back
    # 4. jmp to code entry point

    stub_code = bytearray()

    # Offsets we'll patch later
    # lea _stub(pc), a2         ; 4 bytes
    # lea _code(pc), a3         ; 4 bytes  (code starts after stub + reloc table)
    # lea _reloc(pc), a1        ; 4 bytes

    code_offset = None  # will compute after we know stub+reloc size

    # Build relocation table as 16-bit deltas (more compact than TOS format)
    reloc_table = bytearray()
    prev = 0
    for fixup in fixups:
        delta = fixup - prev
        while delta > 65534:
            reloc_table += struct.pack('>H', 65534)
            reloc_table += struct.pack('>H', 0)  # 0 = skip (no fixup, just advance)
            delta -= 65534
        reloc_table += struct.pack('>H', delta)
        prev = fixup
    reloc_table += struct.pack('>H', 0)  # end marker
    reloc_table += struct.pack('>H', 0)  # padding for alignment

    # Compute sizes
    # Stub is small fixed-size code (~40 bytes)
    # Then relocation table
    # Then code+data

    # Build the stub assembly as machine code bytes:
    #
    # _stub:
    #   lea     _code(pc),a3          ; 41FB xxxx  (PC-relative to code)
    #   lea     _reloc(pc),a1         ; 43FA xxxx  (PC-relative to reloc table)
    #   move.l  a3,a2                 ; 2448       (a2 = code base for fixups)
    # .loop:
    #   move.w  (a1)+,d0              ; 3019       (read 16-bit delta)
    #   beq.s   .done                 ; 6700 xxxx  (0 = end)
    #   add.l   d0,a0                 ; D1C0... wait, we need a0
    #
    # Actually let me just hardcode the stub bytes.

    # Simpler stub approach:
    # We know the exact layout:
    #   [stub code] [reloc table] [code+data]
    # stub code lea's are PC-relative to known offsets.

    stub_size = 36  # we'll make it exactly this
    reloc_offset = stub_size  # reloc table starts right after stub
    code_start_offset = stub_size + len(reloc_table)

    # Ensure code starts at even address
    if code_start_offset & 1:
        reloc_table += b'\x00'
        code_start_offset += 1

    # Now build stub machine code:
    s = bytearray()

    # lea code(pc), a3  -->  47FA disp16
    disp_to_code = code_start_offset - (len(s) + 2)  # PC at instruction + 2
    s += bytes([0x47, 0xFA]) + struct.pack('>h', disp_to_code)

    # lea reloc(pc), a1  -->  43FA disp16
    disp_to_reloc = reloc_offset - (len(s) + 2)
    s += bytes([0x43, 0xFA]) + struct.pack('>h', disp_to_reloc)

    # movea.l a3, a0  -->  2048 + 0BC0... no
    # move.l a3, a0  -->  2048... wait
    # movea.l a3,a0 = 204B
    s += bytes([0x20, 0x4B])  # movea.l a3, a0  (a0 = code base, used as current ptr)

    # Relocation loop:
    # .loop:
    loop_offset = len(s)
    #   move.w (a1)+, d0  -->  3019
    s += bytes([0x30, 0x19])
    #   beq.s .done  -->  6700 xxxx (branch if zero)
    done_branch_pos = len(s)
    s += bytes([0x67, 0x00, 0x00, 0x00])  # placeholder, will patch

    #   ext.l d0  -->  48C0 (sign-extend word to long... but deltas are unsigned)
    #   Actually we want unsigned extend: andi.l #$FFFF, d0  or swap+clr+swap
    #   Simpler: moveq #0,d1; move.w d0,d1; use d1
    #   Or just: and.l #$FFFF, d0
    s += bytes([0x02, 0x80, 0x00, 0x00, 0xFF, 0xFF])  # andi.l #$FFFF, d0

    #   adda.l d0, a0  -->  D1C0
    s += bytes([0xD1, 0xC0])

    #   add.l a3, (a0)  -->  D793  (add.l a3, (a0))
    #   Wait: add.l An,(Am) is not valid. Need: move.l (a0),d1; add.l a3,d1; move.l d1,(a0)
    s += bytes([0x22, 0x10])  # move.l (a0), d1
    s += bytes([0xD2, 0x8B])  # add.l a3, d1
    s += bytes([0x20, 0x81])  # move.l d1, (a0)

    #   move.l a3, a0  -->  204B (reset a0 to code base for next delta)
    #   Wait no! The deltas are cumulative from the start. a0 should track current position.
    #   Actually, the deltas in our table are from the PREVIOUS fixup, not from start.
    #   So a0 advances by delta each time. Let me redo:

    # Oops, the approach is wrong. Let me restart the stub logic.
    # a0 starts at code_base.
    # Each delta: a0 += delta, then patch the long at (a0).
    # So we DON'T reset a0.

    # Let me rebuild properly:
    s = bytearray()

    # lea code(pc), a0  -->  41FA disp16
    disp_to_code = code_start_offset - (len(s) + 2)
    s += bytes([0x41, 0xFA]) + struct.pack('>h', disp_to_code)

    # lea reloc(pc), a1  -->  43FA disp16
    disp_to_reloc = reloc_offset - (len(s) + 2)
    s += bytes([0x43, 0xFA]) + struct.pack('>h', disp_to_reloc)

    # move.l a0, d2  -->  2408  (save code base in d2 for adding to fixups)
    s += bytes([0x24, 0x08])

    # .loop:
    loop_offset = len(s)
    # move.w (a1)+, d0  -->  3019
    s += bytes([0x30, 0x19])
    # beq.s .done
    done_branch_pos = len(s)
    s += bytes([0x67, 0x00])  # beq.s placeholder (8-bit displacement)

    # andi.l #$FFFF, d0  (unsigned extend 16->32)
    s += bytes([0x02, 0x80, 0x00, 0x00, 0xFF, 0xFF])

    # adda.l d0, a0  -->  D1C0
    s += bytes([0xD1, 0xC0])

    # add.l d2, (a0)  -->  D590
    s += bytes([0xD5, 0x90])

    # bra.s .loop
    loop_disp = loop_offset - (len(s) + 2)
    s += bytes([0x60, loop_disp & 0xFF])

    # .done:
    done_offset = len(s)
    # Patch the beq.s
    beq_disp = done_offset - (done_branch_pos + 2)
    s[done_branch_pos + 1] = beq_disp & 0xFF

    # jmp (a0) ... no, jump to entry point
    # The TOS entry point is at offset 0 of the text section.
    # a0 is currently pointing somewhere in the code (last fixup location).
    # We need to jump to code_base.
    # move.l d2, a0  -->  2042
    s += bytes([0x20, 0x42])
    # jmp (a0)  -->  4ED0
    s += bytes([0x4E, 0xD0])

    # Pad to stub_size
    while len(s) < stub_size:
        s += bytes([0x4E, 0x71])  # nop

    if len(s) != stub_size:
        print(f"Warning: stub is {len(s)} bytes, expected {stub_size}")
        stub_size = len(s)
        # Recalculate offsets
        reloc_offset = stub_size
        code_start_offset = stub_size + len(reloc_table)
        if code_start_offset & 1:
            reloc_table += b'\x00'
            code_start_offset += 1
        # Re-patch the lea displacements
        disp_to_code = code_start_offset - 2
        struct.pack_into('>h', s, 2, disp_to_code)
        disp_to_reloc = reloc_offset - 6
        struct.pack_into('>h', s, 6, disp_to_reloc)

    # Build final binary
    binary = bytes(s) + bytes(reloc_table) + code_data

    # Prepend Q-emulator header
    hdr = b']!QDOS File Header'
    hdr += b'\x00'       # reserved
    hdr += bytes([15])   # wordlen (15 words = 30 bytes)
    hdr += bytes([0])    # access
    hdr += bytes([1])    # type = executable
    hdr += struct.pack('>I', bss_size + 4096)  # data space (BSS + stack)
    hdr += struct.pack('>I', 0)   # reserved
    hdr += b'\x00' * (30 - len(hdr))

    # XTcc trailer (8 bytes at end) — for emulators that check file tail
    # instead of Q-emulator header (e.g., iQL)
    xtcc = b'XTcc' + struct.pack('>I', bss_size + 4096)

    with open(outfile, 'wb') as f:
        f.write(hdr)
        f.write(binary)
        f.write(xtcc)

    total = len(hdr) + len(binary) + len(xtcc)
    print(f"Output: {outfile} ({total} bytes)")
    print(f"  Stub: {stub_size}, Reloc: {len(reloc_table)}, Code: {len(code_data)}")
    print(f"  Headers: Q-emulator (start) + XTcc (tail)")


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} input.tos output_ql")
        sys.exit(1)
    code_data, fixups, bss_size = read_tos(sys.argv[1])
    make_qdos_binary(code_data, fixups, bss_size, sys.argv[2])
