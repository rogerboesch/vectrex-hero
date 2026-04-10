# GBC H.E.R.O. — Makefile for GBDK-2020
#
# Set GBDK to your GBDK-2020 install path:
#   export GBDK=/opt/gbdk
#   make
#
# The output is hero.gbc — load in any GBC emulator (BGB, SameBoy, etc.)

GBDK   ?= /Users/roger/retro-tools/gbdk
LCC     = $(GBDK)/bin/lcc

# -Wa-l        = assembler listing
# -Wl-m        = linker map
# -Wl-j        = linker no$gmb symbols
# -Wm-yC       = GBC mode (CGB flag in ROM header)
# -Wm-yn"HERO" = ROM title
# -Wl-yt0x19   = MBC5 cartridge type
# -Wl-yo8      = 4 ROM banks (64KB)
LCCFLAGS = -Wa-l -Wl-m -Wl-j -Wm-yC -Wm-yn"HERO" -Wl-yt0x19 -Wl-yo8

LEVEL_BANK_SRCS = $(wildcard level_data_*.c)
SRCS = main.c player.c enemies.c levels.c render.c tiles.c levels_meta.c \
       tileset_export.c sprites_export.c palettes_export.c screens_export.c \
       $(LEVEL_BANK_SRCS)
TARGET = hero.gbc

$(TARGET): $(SRCS) game.h tiles.h level_data.h
	$(LCC) $(LCCFLAGS) -o $@ $(SRCS)

clean:
	rm -f $(TARGET) *.o *.lst *.map *.noi *.sym *.asm

.PHONY: clean
