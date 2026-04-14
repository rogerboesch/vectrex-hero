# Mega Drive R.E.S.C.U.E. — Makefile for SGDK
#
# Build: make
# Run:   blastem out/rom.bin  (or any MD emulator)

SGDK ?= $(HOME)/retro-tools/sgdk

# SGDK uses its own makefile system
# We wrap it for convenience

GDK_INC  = $(SGDK)/inc
GDK_LIB  = $(SGDK)/lib
GDK_BIN  = $(SGDK)/bin

CC       = $(GDK_BIN)/gcc
OBJCOPY  = $(GDK_BIN)/objcopy
ASMZ80   = $(GDK_BIN)/sjasm
RESCOMP  = java -jar $(SGDK)/bin/rescomp.jar

INCS     = -I$(GDK_INC) -Isrc -Ires
CFLAGS   = -m68000 -Wall -O2 -fno-builtin $(INCS)
LDFLAGS  = -L$(GDK_LIB) -lmd -lgcc -lnosys -T $(SGDK)/md.ld

SRCDIR   = src
RESDIR   = res
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
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(TARGET): $(OUTDIR)/rom.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -rf $(OUTDIR)

.PHONY: all clean
