# Mega Drive R.E.S.C.U.E. — Makefile
#
# Requires:
#   brew install m68k-elf-binutils m68k-elf-gcc
#   SGDK at $HOME/retro-tools/sgdk (for headers + libs)
#
# Build: make
# Run:   blastem out/rom.bin  (or any MD emulator)

SGDK     ?= $(HOME)/retro-tools/sgdk

# Use Homebrew m68k-elf-gcc (falls back to SGDK's bundled gcc)
CC       := $(shell which m68k-elf-gcc 2>/dev/null || echo $(SGDK)/bin/gcc)
OBJCOPY  := $(shell which m68k-elf-objcopy 2>/dev/null || echo $(SGDK)/bin/objcopy)

GDK_INC  = $(SGDK)/inc
GDK_LIB  = $(SGDK)/lib

INCS     = -I$(GDK_INC) -Isrc -Ires
CFLAGS   = -m68000 -Wall -O2 -fno-builtin -ffunction-sections -fdata-sections $(INCS)
LDFLAGS  = -m68000 -L$(GDK_LIB) -lmd -lgcc -T $(SGDK)/md.ld \
           -Wl,--gc-sections -nostdlib

SRCDIR   = src
OUTDIR   = out

SRCS     = $(wildcard $(SRCDIR)/*.c)
OBJS     = $(SRCS:$(SRCDIR)/%.c=$(OUTDIR)/%.o)

TARGET   = $(OUTDIR)/rom.bin

all: $(TARGET)

$(OUTDIR):
	mkdir -p $(OUTDIR)

$(OUTDIR)/%.o: $(SRCDIR)/%.c | $(OUTDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUTDIR)/rom.elf: $(OBJS) | $(OUTDIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

$(TARGET): $(OUTDIR)/rom.elf
	$(OBJCOPY) -O binary $< $@
	@echo "ROM: $@ ($$(wc -c < $@ | tr -d ' ') bytes)"

clean:
	rm -rf $(OUTDIR)

# Export assets from GBC project file
export:
	python3 ../tools/md/export_md.py ../gbc/game-rescue.json src/

.PHONY: all clean export
