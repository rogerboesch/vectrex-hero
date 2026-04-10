# QL Workbench — SDL2-only sprite editor + emulator
#
# Build: make
# Run:   ./ql-workbench [sprites.json]

CC = cc
IQL_DIR = ../iql

SDL_CFLAGS  = $(shell sdl2-config --cflags)

# Static linking: SDL2 + SDL2_ttf + freetype (harfbuzz/graphite dynamic)
SDL_STATIC  = $(shell sdl2-config --static-libs) \
              /opt/homebrew/lib/libSDL2_ttf.a \
              /opt/homebrew/lib/libfreetype.a \
              /opt/homebrew/lib/libpng.a \
              -lharfbuzz -lz -lbz2 -lc++

SHARED = ../../shared

CFLAGS = -std=c11 -O2 -Wall -Wno-unused-function \
         $(SDL_CFLAGS) -I$(SHARED) \
         -I $(IQL_DIR)/QL -I $(IQL_DIR)/base -I $(IQL_DIR)/platform

# iQL needs gnu89 + specific flags
IQL_CFLAGS = -O2 -std=gnu89 -w -Wno-error \
             -Wno-int-conversion -Wno-incompatible-pointer-types \
             -DMOUSE=1 -DHAS_STPCPY -DUSE_BUILTIN_EXPECT \
             $(SDL_CFLAGS) \
             -I $(IQL_DIR)/QL -I $(IQL_DIR)/base -I $(IQL_DIR)/platform

LDFLAGS = $(SDL_STATIC) -lpthread

TARGET = ql-workbench
BUILD  = build

# Shared UI sources
SHARED_SRCS = $(SHARED)/ui.c $(SHARED)/native_macos.m

# App sources
APP_SRCS = src/main.c src/app.c src/file_io.c src/emulator.c \
           src/panel_sprite_list.c src/panel_sprite_canvas.c \
           src/panel_palette.c src/panel_properties.c src/panel_preview.c \
           src/panel_image_list.c src/panel_image_canvas.c src/panel_image_tools.c \
           src/panel_console.c src/panel_emulator.c \
           src/panel_cpu_state.c src/panel_disasm.c src/panel_memory.c \
           src/panel_breakpoints.c src/m68k_disasm.c

# iQL emulator C sources
IQL_SRCS = \
	$(IQL_DIR)/QL/QL_basext.c \
	$(IQL_DIR)/QL/QL_boot.c \
	$(IQL_DIR)/QL/QL_cconv.c \
	$(IQL_DIR)/QL/QL_config.c \
	$(IQL_DIR)/QL/QL_disk.c \
	$(IQL_DIR)/QL/QL_driver.c \
	$(IQL_DIR)/QL/QL_emulator.c \
	$(IQL_DIR)/QL/QL_files.c \
	$(IQL_DIR)/QL/QL_hardware.c \
	$(IQL_DIR)/QL/QL_ip.c \
	$(IQL_DIR)/QL/QL_mouse.c \
	$(IQL_DIR)/QL/QL_poll.c \
	$(IQL_DIR)/QL/QL_screen.c \
	$(IQL_DIR)/QL/QL_serial.c \
	$(IQL_DIR)/QL/QL_serio.c \
	$(IQL_DIR)/QL/QL_sound.c \
	$(IQL_DIR)/QL/QL_traps.c \
	$(IQL_DIR)/QL/QL_vfs.c \
	$(IQL_DIR)/base/base_bdi.c \
	$(IQL_DIR)/base/base_cfg.c \
	$(IQL_DIR)/base/base_dummies.c \
	$(IQL_DIR)/base/base_general.c \
	$(IQL_DIR)/base/base_iexl_general.c \
	$(IQL_DIR)/base/base_init.c \
	$(IQL_DIR)/base/base_instructions_ao.c \
	$(IQL_DIR)/base/base_instructions_pz.c \
	$(IQL_DIR)/base/base_memaccess.c \
	$(IQL_DIR)/base/base_pty.c \
	$(IQL_DIR)/base/base_qmtrap.c \
	$(IQL_DIR)/base/base_script.c \
	$(IQL_DIR)/base/base_trace.c \
	$(IQL_DIR)/base/base_util.c \
	$(IQL_DIR)/base/base_uxfile.c \
	$(IQL_DIR)/base/base_version.c \
	$(IQL_DIR)/base/base_vm.c \
	$(IQL_DIR)/base/base_xc68.c \
	$(IQL_DIR)/base/base_xcodes.c \
	$(IQL_DIR)/platform/rb_screen.c \
	$(IQL_DIR)/platform/rb_settings.c \
	$(IQL_DIR)/platform/rb_virtual_keys.c

# Platform stub
PLATFORM_SRCS = $(IQL_DIR)/platform/iql_platform.c

APP_OBJS = $(patsubst src/%.c,$(BUILD)/%.o,$(APP_SRCS))
SHARED_OBJS = $(BUILD)/shared_ui.o $(BUILD)/shared_native_macos.o
IQL_OBJS = $(patsubst $(IQL_DIR)/%.c,$(BUILD)/iql_%.o,$(IQL_SRCS))
PLATFORM_OBJS = $(BUILD)/iql_platform.o

ALL_OBJS = $(APP_OBJS) $(SHARED_OBJS) $(IQL_OBJS) $(PLATFORM_OBJS)

all: $(TARGET)

$(TARGET): $(ALL_OBJS)
	$(CC) $(CFLAGS) -o $@ $(ALL_OBJS) $(LDFLAGS)

# App C sources
$(BUILD)/%.o: src/%.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# Shared UI sources
$(BUILD)/shared_ui.o: $(SHARED)/ui.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/shared_native_macos.o: $(SHARED)/native_macos.m
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# iQL C sources
$(BUILD)/iql_%.o: $(IQL_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(IQL_CFLAGS) -c $< -o $@

# Platform stub
$(BUILD)/iql_platform.o: $(IQL_DIR)/platform/iql_platform.c
	@mkdir -p $(BUILD)
	$(CC) $(IQL_CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET)

.PHONY: all clean
