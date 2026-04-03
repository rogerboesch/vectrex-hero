# GBC Studio — Tile/sprite editor for Game Boy Color
#
# Build: make
# Run:   ./gbc-studio

CC = cc
SHARED = ../../shared

SDL_CFLAGS  = $(shell sdl2-config --cflags)
SDL_STATIC  = $(shell sdl2-config --static-libs) \
              /opt/homebrew/lib/libSDL2_ttf.a \
              /opt/homebrew/lib/libfreetype.a \
              /opt/homebrew/lib/libpng.a \
              -lharfbuzz -lz -lbz2 -lc++

CFLAGS = -std=c11 -O2 -Wall -Wno-unused-function $(SDL_CFLAGS) -I$(SHARED) -Isrc/gaembuoy
GB_CFLAGS = -std=c11 -O2 -w $(SDL_CFLAGS) -Isrc/gaembuoy
LDFLAGS = $(SDL_STATIC)

TARGET = gbc-studio
BUILD  = build

APP_SRCS = src/main.c src/app.c src/file_io.c src/gbc_emu.c \
           src/panel_tile_list.c src/panel_tile_editor.c src/panel_tile_tools.c \
           src/panel_palette.c src/panel_preview.c src/panel_emulator.c \
           src/panel_console.c

GB_SRCS = src/gaembuoy/cpu.c src/gaembuoy/gpu.c src/gaembuoy/memory.c \
          src/gaembuoy/cart.c src/gaembuoy/irq.c src/gaembuoy/dma.c \
          src/gaembuoy/hdma.c src/gaembuoy/timer.c src/gaembuoy/input.c \
          src/gaembuoy/spu.c src/gaembuoy/sync.c src/gaembuoy/rtc.c

SHARED_SRCS = $(SHARED)/ui.c $(SHARED)/native_macos.m

APP_OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(filter %.c,$(APP_SRCS)))
GB_OBJS = $(patsubst src/gaembuoy/%.c,$(BUILD)/gb_%.o,$(GB_SRCS))
SHARED_OBJS = $(BUILD)/ui.o $(BUILD)/native_macos.o
ALL_OBJS = $(APP_OBJS) $(GB_OBJS) $(SHARED_OBJS)

all: $(TARGET)

$(TARGET): $(ALL_OBJS)
	$(CC) $(CFLAGS) -o $@ $(ALL_OBJS) $(LDFLAGS)

$(BUILD)/%.o: src/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# Gaembuoy emulator sources (relaxed warnings)
$(BUILD)/gb_%.o: src/gaembuoy/%.c
	@mkdir -p $(BUILD)
	$(CC) $(GB_CFLAGS) -c $< -o $@

$(BUILD)/ui.o: $(SHARED)/ui.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/native_macos.o: $(SHARED)/native_macos.m
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET)

.PHONY: all clean
