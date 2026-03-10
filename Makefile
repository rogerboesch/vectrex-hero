VECTREC = $(HOME)/retro-tools/vectrec
CMOC = $(VECTREC)/cmoc
VEC2X = vec2x
STDLIB = $(VECTREC)/stdlib
ROM = $(VECTREC)/roms/romfast.bin
OVERLAY = $(VECTREC)/roms/empty.png

TARGET = main
SRC = main.c player.c enemies.c drawing.c levels.c
BIN = $(TARGET).bin

all: $(BIN)

$(BIN): $(SRC) hero.h sprites.h levels.h
	$(CMOC) -I$(STDLIB) -L$(STDLIB) --vectrex --verbose -o $(BIN) $(SRC)

run: $(BIN)
	$(VEC2X) right $(ROM) $(BIN) $(OVERLAY)

clean:
	rm -f $(BIN) *.lst *.i *.asm *.o *.link *.map

.PHONY: all run clean
