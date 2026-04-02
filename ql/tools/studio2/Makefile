# QL Studio 2 — SDL2-only sprite editor
#
# Build: make
# Run:   ./qlstudio2 [sprites.json]

CC = cc

SDL_CFLAGS  = $(shell sdl2-config --cflags)

# Static linking: SDL2 + SDL2_ttf + freetype (harfbuzz/graphite dynamic)
SDL_STATIC  = $(shell sdl2-config --static-libs) \
              /opt/homebrew/lib/libSDL2_ttf.a \
              /opt/homebrew/lib/libfreetype.a \
              /opt/homebrew/lib/libpng.a \
              -lharfbuzz -lz -lbz2 -lc++

CFLAGS = -std=c11 -O2 -Wall -Wno-unused-function $(SDL_CFLAGS)
LDFLAGS = $(SDL_STATIC)

TARGET = qlstudio2
BUILD  = build

SRCS = src/main.c src/ui.c src/app.c
OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(BUILD)/%.o: src/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET)

.PHONY: all clean
