VECTREC = $(HOME)/retro-tools/vectrec
CMOC = $(VECTREC)/cmoc
VEC2X = vec2x
STDLIB = $(VECTREC)/stdlib
ROM = $(VECTREC)/roms/romfast.bin
OVERLAY = $(VECTREC)/roms/empty.png

SRCDIR = src
BINDIR = bin

TARGET = main
SRC = $(SRCDIR)/main.c $(SRCDIR)/player.c $(SRCDIR)/enemies.c $(SRCDIR)/drawing.c $(SRCDIR)/levels.c
HDR = $(SRCDIR)/hero.h $(SRCDIR)/sprites.h $(SRCDIR)/levels.h
BIN = $(BINDIR)/$(TARGET).bin

all: $(BIN)

$(BIN): $(SRC) $(HDR) | $(BINDIR)
	$(CMOC) -I$(STDLIB) -L$(STDLIB) --vectrex --verbose -o $(BIN) $(SRC)

$(BINDIR):
	mkdir -p $(BINDIR)

run: $(BIN)
	$(VEC2X) right $(ROM) $(BIN) $(OVERLAY)

clean:
	rm -f $(BINDIR)/*.bin $(BINDIR)/*.link $(BINDIR)/*.map *.lst *.i *.asm *.o *.s *.link *.map

.PHONY: all run clean
