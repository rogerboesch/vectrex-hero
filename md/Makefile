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

# Export assets from GBC project file
export:
	python3 ../tools/md/export_md.py ../gbc/game-rescue.json src/

.PHONY: export
