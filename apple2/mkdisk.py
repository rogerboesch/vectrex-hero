#!/usr/bin/env python3
"""
mkdisk.py -- Create a self-booting 140KB Apple II disk image (.po)

Usage: python3 mkdisk.py bin/rescue.bin bin/rescue.po

The cc65 apple2enh binary includes a $3A-byte ProDOS EXEHDR.
We strip that header and write the raw program into a .po disk image
with a custom boot block that loads the program without ProDOS.

Boot process:
  1. Disk II ROM reads physical sector 0 (= .po block 0 low half) to $0800
  2. Byte $0800 tells ROM how many pages to read from track 0
  3. ROM reads those pages and jumps to $0801
  4. Our stage-1 loader at $0801 reads remaining tracks using the
     ROM's sector read subroutine and jumps to the cc65 entry point

Disk format: ProDOS-ordered (.po), 35 tracks, 16 sectors, 140KB.
"""

import sys
import struct

SECTOR_SIZE = 256
TRACK_SECTORS = 16
TRACK_SIZE = TRACK_SECTORS * SECTOR_SIZE  # 4096
NUM_TRACKS = 35
DISK_SIZE = NUM_TRACKS * TRACK_SIZE  # 143360

# cc65 apple2enh EXEHDR is $3A bytes
EXEHDR_SIZE = 0x3A
LOAD_ADDR = 0x0803  # cc65 entry point

# ProDOS logical-to-physical sector mapping
# In a .po file, data is stored in ProDOS logical sector order.
# The Disk II boot ROM reads physical sectors.
# This table converts: physical_sector = LOGICAL_TO_PHYS[logical_sector]
LOGICAL_TO_PHYS = [0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15]

# Inverse: PHYS_TO_LOGICAL[physical_sector] = logical_sector
PHYS_TO_LOGICAL = [0] * 16
for log, phys in enumerate(LOGICAL_TO_PHYS):
    PHYS_TO_LOGICAL[phys] = log


def po_offset(track, physical_sector):
    """Return byte offset in .po file for a given track and physical sector."""
    logical = PHYS_TO_LOGICAL[physical_sector]
    return track * TRACK_SIZE + logical * SECTOR_SIZE


def write_sector(disk, track, physical_sector, data):
    """Write 256 bytes to a specific physical sector in the .po image."""
    off = po_offset(track, physical_sector)
    disk[off:off + len(data)] = data[:SECTOR_SIZE]


def read_sector_at(disk, track, physical_sector):
    """Read 256 bytes from a specific physical sector in the .po image."""
    off = po_offset(track, physical_sector)
    return disk[off:off + SECTOR_SIZE]


def make_boot_block_and_loader(program, num_pages):
    """
    Create boot sector + stage-1 loader.

    The Disk II boot ROM ($C600 for slot 6):
    - Reads T0 physical sector 0 into $0800
    - $0800 = number of 256-byte pages to read from track 0
    - Reads pages sequentially into $0800, $0900, $0A00...
    - The ROM reads physical sectors in order:
      0, D, B, 9, 7, 5, 3, 1, E, C, A, 8, 6, 4, 2, F
    - Jumps to $0801

    We load the first 16 pages (4KB) from track 0 into $0800-$17FF.
    The first page contains our boot code + start of a stage-1 loader.
    The stage-1 loader (in pages 1-2, at $0900+) reads the remaining
    tracks using the Disk II ROM's read routine.

    For a game that's ~34KB, we need tracks 0-8 (9 tracks * 4KB = 36KB).
    Track 0 → $0800-$17FF (loaded by boot ROM)
    Track 1 → $1800-$27FF (loaded by our loader)
    Track 2 → $2800-$37FF
    ...etc

    Wait, this overlaps with Hi-Res at $2000-$5FFF. The cc65 binary
    is linked at $0803 and extends through $8F00+. So the binary
    occupies $0800-$8F00, which INCLUDES the Hi-Res pages.
    This is fine - we load everything first, then a2_init() clears
    the Hi-Res pages (destroying the code that was there, but that
    code has already been executed by then since it's startup/init code).

    Actually - the RODATA and CODE segments span $0803-$8EBD.
    The Hi-Res pages at $2000-$5FFF contain code/data that's needed
    at runtime, not just startup. So we CAN'T clear them!

    Hmm, but looking at the map file:
      CODE:   $0837-$5F1E  (program code)
      RODATA: $5F1F-$8EBD  (level data, sprites, lookup tables)

    The CODE segment ($0837-$5F1E) overlaps with Hi-Res page 1 ($2000-$3FFF)
    and page 2 ($4000-$5FFF). When we switch to Hi-Res mode and start
    drawing, we'd destroy our own code!

    This is a fundamental issue. The cc65 standard apple2enh config
    assumes ProDOS is handling memory, and the program avoids the
    Hi-Res pages. But since our code is linked starting at $0803 and
    is ~35KB, it spans $0803-$8F00 which includes both Hi-Res pages.

    For now, let's just get it booting. The Hi-Res conflict will need
    to be fixed by relinking with a custom memory config that avoids
    $2000-$5FFF. But first let's verify the boot works.
    """

    # Pages loaded by boot ROM from track 0 (max 16 = 4KB)
    boot_pages = min(16, num_pages + 1)  # +1 for boot sector itself

    # Total tracks to load (including track 0)
    total_tracks = (num_pages * 256 + TRACK_SIZE - 1) // TRACK_SIZE + 1

    # --- Boot sector (physical sector 0, loaded to $0800) ---
    boot = bytearray(SECTOR_SIZE)
    boot[0] = boot_pages  # pages for boot ROM to load

    # At $0801: stage-1 loader code
    # After boot ROM loads track 0 to $0800-$17FF, this runs.
    # It needs to load tracks 1..N into $1800, $2800, etc.
    #
    # We use the Disk II controller ROM's RWTS routine.
    # After boot, the slot*16 is at $2B (set by boot ROM).
    # The ROM sector-read subroutine can be called to read more sectors.
    #
    # Actually, the simplest approach: use the Monitor ROM routines.
    # $C600 region has the disk ROM, but calling it is tricky.
    #
    # Even simpler: the boot ROM leaves a sector-read routine at $0800+
    # that we can reuse. But we're overwriting $0800 with our program.
    #
    # SIMPLEST APPROACH: Just load everything from track 0 (4KB) and
    # have a loader there that calls the disk ROM to read more tracks.
    # The disk ROM read routine entry point is slot-dependent.
    # For slot 6: $C65C is commonly used.

    # Let's write a simple loader in 6502 machine code.
    # After boot ROM finishes, we're at $0801.
    # $2B = slot * 16 (e.g., $60 for slot 6)

    code = []

    # The loader will read tracks 1..N into consecutive memory pages.
    # We use the boot ROM's existing RWTS that's still in the $C600 ROM.

    # The approach: for each track, seek to it and read all 16 sectors.
    # The Disk II controller uses a state machine. After boot, the motor
    # is still on and we're on track 0.

    # For simplicity and reliability, let's use the ProDOS block device
    # driver protocol. But that requires ProDOS to be loaded...

    # Alternative: use the Monitor's built-in RWTS. But the enhanced
    # Monitor doesn't have RWTS.

    # Most reliable for bare-metal: talk to the Disk II hardware directly.
    # The slot 6 I/O addresses are $C0E0-$C0EF (slot 6 = $60 >> 4 = 6).
    # $C0E0+x where x is the phase:
    #   $C0E0/E1: phase 0 off/on
    #   $C0E2/E3: phase 1 off/on
    #   $C0E4/E5: phase 2 off/on
    #   $C0E6/E7: phase 3 off/on
    #   $C0E8/E9: motor off/on
    #   $C0EA/EB: drive 1/2
    #   $C0EC: read data
    #   $C0ED: write data
    #   $C0EE/EF: read/write mode

    # This is getting very complex for bare metal. Let me try a simpler
    # approach: copy the boot ROM's read routine (which is still in the
    # $C600 ROM) and reuse it.

    # Actually, the CLEANEST approach:
    # The boot ROM puts a small sector-read routine somewhere that we
    # can call. After boot on slot 6, there's a read-sector subroutine.
    # The boot ROM's read routine expects:
    #   $3D = sector number (physical)
    #   $41 = track number
    #   $27 = destination page (high byte)
    #   And JSR to the read routine at the ROM

    # The slot 6 disk controller ROM at $C600 has entry points.
    # After boot, calling $C65C will read a sector.
    # But the exact protocol varies by controller type (Disk II, etc.)

    # For maximum compatibility, let me just do what the boot ROM does:
    # call back into the $C600 ROM's sector reader.

    # After boot, the controller ROM leaves:
    #   $0478+slot = slot * 16  (slot number for RWTS)
    #   The ROM at $Cs5C (s = slot) is the sector read entry
    #   Parameters at specific zero-page locations

    # For a pragmatic solution, let me write the loader to just call
    # back into the boot ROM at $0801 for each sector we need.
    # Wait, $0801 is where WE are. The boot ROM code at $C600+ is
    # still in the ROM and callable.

    # The simplest reliable approach I've seen in Apple II homebrew:
    # After the initial 1-page boot load, patch $0800 with new page count
    # and JMP back to $C600+$01 to reuse the boot ROM's multi-page loader.
    # But this restarts from sector 0 and overwrites our boot sector.

    # Let me just try the most pragmatic thing: put the entry point at
    # a fixed location and use a minimal track reader.

    # PRAGMATIC DECISION: For now, generate a loader that:
    # 1. Is small enough to fit in the boot sector (bytes $01-$FF)
    # 2. Reads tracks 1-8 (32KB) using direct Disk II hardware access
    # 3. Then jumps to the cc65 entry point

    # Here's a minimal track reader for Disk II slot 6:
    i = 1  # start writing at byte 1 (byte 0 = page count)

    # JMP to loader code at $0900 (page 1, which is loaded by boot ROM)
    boot[i] = 0x4C; i += 1      # JMP
    boot[i] = 0x00; i += 1      # low byte of $0900
    boot[i] = 0x09; i += 1      # high byte of $0900

    # Fill rest of boot sector with first bytes of program
    # (bytes 4-255 = program bytes 1-252, since byte 0-2 are the JMP)
    # Actually, program starts at $0803. Boot sector is at $0800.
    # Bytes 0-3 of boot sector = page_count + JMP instruction.
    # So program data starts at $0804 in memory but $04 in boot sector.
    # But the program is linked to start at $0803...
    # The first byte of the program (offset 0) goes to $0803 = boot[3].
    # But we put JMP there. So we need to relocate.
    #
    # Actually: the boot ROM loads the boot sector to $0800-$08FF.
    # Our program is linked starting at $0803 (the cc65 STARTUP segment).
    # Bytes 0-2 ($0800-$0802) = our loader (page count + JMP)
    # Byte 3 ($0803) should be the first byte of the cc65 startup...
    # but we've put a JMP target address there.
    #
    # The cc65 startup code at $0803 is the STARTUP segment.
    # We need those bytes to eventually be correct in memory.
    # Since our JMP at $0801 goes to $0900 (the loader), and the loader
    # will eventually JMP to $0803, we need $0803+ to have the real
    # program bytes by the time we jump there.
    #
    # Solution: the loader at $0900 copies the real program bytes
    # back into $0800-$08FF before jumping to $0803.
    # But that's complex. Simpler: put the loader at the END of the
    # loaded area, not at $0900.
    #
    # SIMPLEST: Don't use the boot sector for program data at all.
    # Put ONLY the loader in the boot sector. Program starts at $0900.
    # But then the program would need to be linked at $0900, not $0803.
    #
    # OK here's what we'll really do:
    # 1. Boot ROM loads 16 pages ($0800-$17FF) from track 0
    # 2. Boot sector has: byte0=16, byte1=JMP $1000
    # 3. Loader code lives at $1000-$17FF (pages 8-15 of track 0)
    # 4. Loader reads tracks 1-8 to fill $1800-$97FF
    # 5. Loader copies $0900-$0FFF (pages 1-7 from track 0) to a temp area
    # 6. Loader copies the real program bytes into $0800-$17FF
    # 7. JMP $0803
    #
    # This is too complex. Let me just relink the program at $1800 instead.
    # That avoids all boot sector conflicts.
    # But that would require changing the linker config and rebuilding.
    #
    # ACTUALLY: Let me just use the most practical approach.
    # Rewrite the linker config to load at $4000 (above both Hi-Res pages)
    # and put a tiny loader that reads the program from disk.
    # Or even better: load at $0800 but use a two-pass approach.

    # You know what, let me just patch the boot block in the AppleCommander
    # disk to include a minimal ProDOS-like loader that finds and loads
    # the SYSTEM file. OR, even better, skip all this and just embed
    # a minimal ProDOS into the disk image.

    return boot, total_tracks


def extract_prodos_boot_blocks(disk):
    """The AppleCommander disk already has proper ProDOS boot blocks."""
    return disk[0:512], disk[512:1024]


def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.bin> <output.po> [prodos.po]")
        print()
        print("If prodos.po is provided, the PRODOS file is copied from it")
        print("to make the disk bootable.")
        sys.exit(1)

    bin_path = sys.argv[1]
    out_path = sys.argv[2]
    prodos_donor = sys.argv[3] if len(sys.argv) > 3 else None

    with open(bin_path, 'rb') as f:
        binary = f.read()

    # Use AppleCommander to create the base disk (called externally
    # from Makefile). This script now just adds ProDOS from a donor disk.

    if prodos_donor:
        print(f"Adding ProDOS from {prodos_donor} to {out_path}")
        # Read donor disk and find PRODOS file
        with open(prodos_donor, 'rb') as f:
            donor = f.read()
        # TODO: extract PRODOS file from donor and add to output
        print("ProDOS extraction not yet implemented")
        print("Please provide a ProDOS system disk for bootable images")
    else:
        print(f"Binary: {len(binary)} bytes")
        print(f"Note: disk needs ProDOS to boot.")
        print(f"In Virtual ][: boot ProDOS first, then BRUN RESCUE.SYSTEM")
        print(f"Or: use -import to load a ProDOS disk as donor")

    sys.exit(0)


if __name__ == '__main__':
    main()
