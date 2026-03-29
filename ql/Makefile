# QL H.E.R.O. — Makefile
#
# Pipeline: vbccm68k (C→asm) → vasm (asm→obj) → vlink (→TOS exe) → tos2ql.py (→QDOS)
#
# Build: make
# Output: hero (QDOS executable with self-relocating loader)

VBCC_DIR ?= /Users/roger/retro-tools/atarist-sdk/vbcc
VBCC     = $(VBCC_DIR)/bin/vbccm68k
VASM     = /Users/roger/retro-tools/vasm/vasm68k_mot
VLINK    = $(VBCC_DIR)/bin/vlink
VBCC_LIB = $(VBCC_DIR)/targets/m68k-atari/lib
INCLUDES = -I$(VBCC_DIR)/targets/m68k-atari/include -I. -I..

VBCC_FLAGS = -quiet -c99 -cpu=68000 -O=1 $(INCLUDES)
VASM_FLAGS = -Fvobj -m68000 -quiet
VLINK_FLAGS = -bataritos -EB -L$(VBCC_LIB)

TARGET = hero

C_SRCS = main.c player.c enemies.c levels.c render.c sprites.c
S_SRCS = crt0.s ql_hw.s

C_OBJS = $(C_SRCS:.c=.o)
S_OBJS = $(S_SRCS:.s=.o)
OBJS   = $(C_OBJS) $(S_OBJS)

all: $(TARGET)

# Step 1: Link as TOS executable (has relocation table)
hero.tos: $(OBJS)
	$(VLINK) $(VLINK_FLAGS) -o $@ crt0.o $(C_OBJS) ql_hw.o -lvc

# Step 2: Convert to self-relocating QDOS binary
$(TARGET): hero.tos tos2ql.py
	python3 tos2ql.py hero.tos $(TARGET)

# C → asm → obj
main.o: main.c game.h sprites.h ql_hw.h
	$(VBCC) $(VBCC_FLAGS) $< -o=main.asm && $(VASM) $(VASM_FLAGS) -o $@ main.asm

player.o: player.c game.h
	$(VBCC) $(VBCC_FLAGS) $< -o=player.asm && $(VASM) $(VASM_FLAGS) -o $@ player.asm

enemies.o: enemies.c game.h
	$(VBCC) $(VBCC_FLAGS) $< -o=enemies.asm && $(VASM) $(VASM_FLAGS) -o $@ enemies.asm

levels.o: levels.c game.h
	$(VBCC) $(VBCC_FLAGS) $< -o=levels.asm && $(VASM) $(VASM_FLAGS) -o $@ levels.asm

render.o: render.c game.h sprites.h
	$(VBCC) $(VBCC_FLAGS) $< -o=render.asm && $(VASM) $(VASM_FLAGS) -o $@ render.asm

sprites.o: sprites.c sprites.h game.h
	$(VBCC) $(VBCC_FLAGS) $< -o=sprites.asm && $(VASM) $(VASM_FLAGS) -o $@ sprites.asm

crt0.o: crt0.s
	$(VASM) $(VASM_FLAGS) -o $@ $<

ql_hw.o: ql_hw.s
	$(VASM) $(VASM_FLAGS) -o $@ $<

clean:
	rm -f $(OBJS) *.asm hero.tos hero.raw $(TARGET)

deploy: $(TARGET)
	cp $(TARGET) /Users/roger/Documents/sqlux/win1/

.PHONY: all clean deploy
