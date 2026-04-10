VECTREC = $(HOME)/retro-tools/vectrec
CMOC = $(VECTREC)/cmoc
VEC2X = vec2x
STDLIB = $(VECTREC)/stdlib
ROM = $(VECTREC)/roms/romfast.bin
OVERLAY = $(VECTREC)/roms/empty.png

SRCDIR = src
BINDIR = bin

TARGET = rescue
SRC = $(SRCDIR)/main.c $(SRCDIR)/player.c $(SRCDIR)/enemies.c $(SRCDIR)/drawing.c $(SRCDIR)/levels.c $(SRCDIR)/sprites.c $(SRCDIR)/font.c
HDR = $(SRCDIR)/hero.h $(SRCDIR)/sprites.h $(SRCDIR)/levels.h $(SRCDIR)/font.h
BIN = $(BINDIR)/$(TARGET).bin

all: $(BIN)

$(BIN): $(SRC) $(HDR) | $(BINDIR)
	$(CMOC) -I$(STDLIB) -L$(STDLIB) --vectrex --verbose --intermediate --intdir=$(BINDIR) -o $(BIN) $(SRC)

$(BINDIR):
	mkdir -p $(BINDIR)

run: $(BIN)
	python3 tools/level_editor.py --rom $(ROM) --cart $(BIN)

clean:
	rm -rf $(BINDIR)

stats: $(BIN)
	@bin_size=$$(wc -c < $(BIN) | tr -d ' '); \
	echo "=== ROM ==="; \
	echo "  Size:  $$bin_size / 32768 bytes ($$(echo "scale=1; $$bin_size*100/32768" | bc)%)"; \
	echo "  Free:  $$((32768 - bin_size)) bytes"; \
	echo ""; \
	echo "=== Code by module ==="; \
	grep "^Section: code" $(BINDIR)/main.map | while IFS= read -r line; do \
		module=$$(echo "$$line" | sed 's/.*(\(.*\)).*/\1/'); \
		len_hex=$$(echo "$$line" | sed 's/.*length //'); \
		len_dec=$$((16#$$len_hex)); \
		if [ $$len_dec -gt 0 ]; then \
			printf "  %-25s %5d bytes\n" "$$module" "$$len_dec"; \
		fi; \
	done; \
	echo ""; \
	echo "=== Const data (rodata) ==="; \
	grep "^Section: rodata" $(BINDIR)/main.map | while IFS= read -r line; do \
		module=$$(echo "$$line" | sed 's/.*(\(.*\)).*/\1/'); \
		len_hex=$$(echo "$$line" | sed 's/.*length //'); \
		len_dec=$$((16#$$len_hex)); \
		if [ $$len_dec -gt 0 ]; then \
			printf "  %-25s %5d bytes\n" "$$module" "$$len_dec"; \
		fi; \
	done; \
	echo ""; \
	bss_total=0; \
	for len_hex in $$(grep "^Section: bss" $(BINDIR)/main.map | sed 's/.*length //'); do \
		bss_total=$$((bss_total + 16#$$len_hex)); \
	done; \
	echo "=== RAM ==="; \
	echo "  Used:  $$bss_total / 896 bytes ($$(echo "scale=1; $$bss_total*100/896" | bc)%)"

TESTBIN = $(BINDIR)/test.bin

test:
	@if [ -z "$(LEVEL)" ]; then echo "Usage: make test LEVEL=3"; exit 1; fi
	mkdir -p $(BINDIR)
	$(CMOC) -I$(STDLIB) -L$(STDLIB) --vectrex --verbose --intermediate --intdir=$(BINDIR) -DSTART_LEVEL=$$(($(LEVEL)-1)) -o $(TESTBIN) $(SRC)
	python3 tools/level_editor.py --rom $(ROM) --cart $(TESTBIN)

.PHONY: all run clean stats test
