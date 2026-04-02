# Vectrex Studio — Level, sprite editor + emulator
#
# Build: make
# Run:   ./vectrex-studio [hero.json]

CC = cc
SHARED = ../../../shared/ui

SDL_CFLAGS  = $(shell sdl2-config --cflags)
SDL_STATIC  = $(shell sdl2-config --static-libs) \
              /opt/homebrew/lib/libSDL2_ttf.a \
              /opt/homebrew/lib/libfreetype.a \
              /opt/homebrew/lib/libpng.a \
              -lharfbuzz -lz -lbz2 -lc++

CFLAGS = -std=c11 -O2 -Wall -Wno-unused-function $(SDL_CFLAGS) -I$(SHARED)
LDFLAGS = $(SDL_STATIC)

TARGET = vectrex-studio
BUILD  = build

APP_SRCS = src/main.c src/app.c src/file_io.c \
           src/panel_level_list.c src/panel_room_editor.c src/panel_room_tools.c \
           src/panel_row_editor.c src/panel_row_tools.c \
           src/panel_sprite_editor.c src/panel_sprite_tools.c \
           src/panel_level_viewer.c src/panel_emu.c \
           src/panel_console.c

SHARED_SRCS = $(SHARED)/ui.c $(SHARED)/native_macos.m

APP_OBJS_C = $(patsubst src/%.c,$(BUILD)/%.o,$(filter %.c,$(APP_SRCS)))
SHARED_OBJS_C = $(BUILD)/ui.o
SHARED_OBJS_M = $(BUILD)/native_macos.o
ALL_OBJS = $(APP_OBJS_C) $(SHARED_OBJS_C) $(SHARED_OBJS_M)

all: $(TARGET)

$(TARGET): $(ALL_OBJS)
	$(CC) $(CFLAGS) -o $@ $(ALL_OBJS) $(LDFLAGS)

$(BUILD)/%.o: src/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/ui.o: $(SHARED)/ui.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/native_macos.o: $(SHARED)/native_macos.m
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET)

.PHONY: all clean
