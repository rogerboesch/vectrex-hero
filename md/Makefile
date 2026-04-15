# Mega Drive R.E.S.C.U.E. — Uses SGDK build system
#
# Requires:
#   brew install m68k-elf-binutils m68k-elf-gcc
#   SGDK at $HOME/retro-tools/sgdk
#
# Build: make
# Run:   blastem out/rom.bin

GDK ?= $(HOME)/retro-tools/sgdk

# Use SGDK's official build system
include $(GDK)/makefile.gen

# Pad ROM and fix checksum (replaces Java sizebnd.jar)
pad: $(OUT_DIR)/rom.bin
	python3 -c "import struct; d=bytearray(open('$(OUT_DIR)/rom.bin','rb').read()); \
	t=128*1024; d+=b'\0'*(t-len(d)) if len(d)<t else b''; \
	c=sum(struct.unpack('>H',d[i:i+2])[0] for i in range(0x200,len(d),2))&0xFFFF; \
	struct.pack_into('>H',d,0x18E,c); \
	open('out/rom.bin','wb').write(d); \
	print(f'ROM: {len(d)} bytes, checksum 0x{c:04X}')"

# Export assets from GBC project file
export:
	python3 ../tools/md/export_md.py ../gbc/game-rescue.json src/

.PHONY: export pad
