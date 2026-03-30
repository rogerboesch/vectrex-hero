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
BUILD  = build

C_SRCS = main.c player.c enemies.c levels.c render.c sprites.c
S_SRCS = crt0.s ql_hw.s

C_OBJS = $(addprefix $(BUILD)/,$(C_SRCS:.c=.o))
S_OBJS = $(addprefix $(BUILD)/,$(S_SRCS:.s=.o))
C_ASMS = $(addprefix $(BUILD)/,$(C_SRCS:.c=.asm))

all: $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

# Step 1: Link as TOS executable (has relocation table)
$(BUILD)/hero.tos: $(C_OBJS) $(S_OBJS)
	$(VLINK) $(VLINK_FLAGS) -o $@ $(BUILD)/crt0.o $(C_OBJS) $(BUILD)/ql_hw.o -lvc

# Step 2: Convert to self-relocating QDOS binary
$(TARGET): $(BUILD)/hero.tos tos2ql.py
	python3 tos2ql.py $(BUILD)/hero.tos $(TARGET)

# C → asm → obj (all go into build/)
$(BUILD)/main.o: main.c game.h sprites.h ql_hw.h | $(BUILD)
	$(VBCC) $(VBCC_FLAGS) $< -o=$(BUILD)/main.asm && $(VASM) $(VASM_FLAGS) -o $@ $(BUILD)/main.asm

$(BUILD)/player.o: player.c game.h | $(BUILD)
	$(VBCC) $(VBCC_FLAGS) $< -o=$(BUILD)/player.asm && $(VASM) $(VASM_FLAGS) -o $@ $(BUILD)/player.asm

$(BUILD)/enemies.o: enemies.c game.h | $(BUILD)
	$(VBCC) $(VBCC_FLAGS) $< -o=$(BUILD)/enemies.asm && $(VASM) $(VASM_FLAGS) -o $@ $(BUILD)/enemies.asm

$(BUILD)/levels.o: levels.c game.h | $(BUILD)
	$(VBCC) $(VBCC_FLAGS) $< -o=$(BUILD)/levels.asm && $(VASM) $(VASM_FLAGS) -o $@ $(BUILD)/levels.asm

$(BUILD)/render.o: render.c game.h sprites.h | $(BUILD)
	$(VBCC) $(VBCC_FLAGS) $< -o=$(BUILD)/render.asm && $(VASM) $(VASM_FLAGS) -o $@ $(BUILD)/render.asm

$(BUILD)/sprites.o: sprites.c sprites.h game.h | $(BUILD)
	$(VBCC) $(VBCC_FLAGS) $< -o=$(BUILD)/sprites.asm && $(VASM) $(VASM_FLAGS) -o $@ $(BUILD)/sprites.asm

$(BUILD)/crt0.o: crt0.s | $(BUILD)
	$(VASM) $(VASM_FLAGS) -o $@ $<

$(BUILD)/ql_hw.o: ql_hw.s | $(BUILD)
	$(VASM) $(VASM_FLAGS) -o $@ $<

clean:
	rm -rf $(BUILD) $(TARGET)

.PHONY: all clean
