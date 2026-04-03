# Vectrex Studio — Level, sprite editor + emulator
#
# Build: make
# Run:   ./vectrex-studio [hero.json]

CC = cc
SHARED = ../../shared

SDL_CFLAGS  = $(shell sdl2-config --cflags)
SDL_STATIC  = $(shell sdl2-config --static-libs) \
              /opt/homebrew/lib/libSDL2_ttf.a \
              /opt/homebrew/lib/libfreetype.a \
              /opt/homebrew/lib/libpng.a \
              -lharfbuzz -lz -lbz2 -lc++

CFLAGS = -std=c11 -O2 -Wall -Wno-unused-function $(SDL_CFLAGS) -I$(SHARED) -I../ivec
VEC2X_CFLAGS = -std=c99 -O2 -w $(SDL_CFLAGS) -I../ivec
LDFLAGS = $(SDL_STATIC)

TARGET = vectrex-studio
BUILD  = build

APP_SRCS = src/main.c src/app.c src/file_io.c src/vectrex_emu.c \
           src/panel_level_list.c src/panel_room_editor.c src/panel_room_tools.c \
           src/panel_row_editor.c src/panel_row_tools.c \
           src/panel_sprite_editor.c src/panel_sprite_tools.c \
           src/panel_level_viewer.c src/panel_emu.c \
           src/panel_vbp.c src/panel_vcpu.c src/panel_vmem.c src/panel_vdisasm.c \
           src/m6809_disasm.c src/panel_console.c

VEC2X_SRCS = ../ivec/vec2x.c ../ivec/e6809.c ../ivec/e8910.c

SHARED_SRCS = $(SHARED)/ui.c $(SHARED)/native_macos.m

APP_OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(filter %.c,$(APP_SRCS)))
VEC2X_OBJS = $(BUILD)/vec2x.o $(BUILD)/e6809.o $(BUILD)/e8910.o
SHARED_OBJS = $(BUILD)/ui.o $(BUILD)/native_macos.o
ALL_OBJS = $(APP_OBJS) $(VEC2X_OBJS) $(SHARED_OBJS)

all: $(TARGET)

$(TARGET): $(ALL_OBJS)
	$(CC) $(CFLAGS) -o $@ $(ALL_OBJS) $(LDFLAGS)

$(BUILD)/%.o: src/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# Vec2x emulator sources (relaxed C standard)
$(BUILD)/vec2x.o: ../ivec/vec2x.c
	@mkdir -p $(BUILD)
	$(CC) $(VEC2X_CFLAGS) -c $< -o $@

$(BUILD)/e6809.o: ../ivec/e6809.c
	@mkdir -p $(BUILD)
	$(CC) $(VEC2X_CFLAGS) -c $< -o $@

$(BUILD)/e8910.o: ../ivec/e8910.c
	@mkdir -p $(BUILD)
	$(CC) $(VEC2X_CFLAGS) -c $< -o $@

$(BUILD)/ui.o: $(SHARED)/ui.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/native_macos.o: $(SHARED)/native_macos.m
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET)

.PHONY: all clean
