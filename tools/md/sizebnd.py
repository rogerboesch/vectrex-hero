#!/usr/bin/env python3
"""Replacement for sizebnd.jar — pad ROM and fix checksum."""
import struct, sys

args = sys.argv[1:]
rom_path = args[0]
size_align = 131072
do_checksum = False

i = 1
while i < len(args):
    if args[i] == "-sizealign":
        size_align = int(args[i + 1])
        i += 2
    elif args[i] == "-checksum":
        do_checksum = True
        i += 1
    else:
        i += 1

with open(rom_path, "rb") as f:
    data = bytearray(f.read())

# Pad to alignment
if len(data) < size_align:
    data += b"\0" * (size_align - len(data))

# Fix checksum
if do_checksum and len(data) >= 0x200:
    checksum = 0
    for i in range(0x200, len(data), 2):
        checksum += struct.unpack(">H", data[i:i+2])[0]
    checksum &= 0xFFFF
    struct.pack_into(">H", data, 0x18E, checksum)

with open(rom_path, "wb") as f:
    f.write(data)
