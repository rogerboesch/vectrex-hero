# QL Studio — ImGui + SDL2 + iQL emulator
#
# Build: make
# Run: ./qlstudio [project.json]

CC = cc
CXX = c++
IQL_DIR = ../iql

SDL_CFLAGS = $(shell sdl2-config --cflags)
SDL_LIBS = $(shell sdl2-config --libs)

CXXFLAGS = -std=c++17 -O2 -Wall -Wno-unused-variable \
           -I thirdparty/imgui $(SDL_CFLAGS) \
           -I $(IQL_DIR)/QL -I $(IQL_DIR)/base -I $(IQL_DIR)/platform

CFLAGS = -O2 -std=gnu89 -w -Wno-error \
         -Wno-int-conversion -Wno-incompatible-pointer-types \
         -DMOUSE=1 -DHAS_STPCPY -DUSE_BUILTIN_EXPECT \
         $(SDL_CFLAGS) \
         -I $(IQL_DIR)/QL -I $(IQL_DIR)/base -I $(IQL_DIR)/platform

LDFLAGS = $(SDL_LIBS) -framework OpenGL -lpthread

TARGET = qlstudio
BUILD = build

# ImGui sources
IMGUI_SRCS = \
	thirdparty/imgui/imgui.cpp \
	thirdparty/imgui/imgui_draw.cpp \
	thirdparty/imgui/imgui_tables.cpp \
	thirdparty/imgui/imgui_widgets.cpp \
	thirdparty/imgui/imgui_demo.cpp \
	thirdparty/imgui/imgui_impl_sdl2.cpp \
	thirdparty/imgui/imgui_impl_opengl3.cpp

# App C++ sources
APP_SRCS = \
	src/main.cpp \
	src/app.cpp \
	src/emulator.cpp

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

# Platform stub (our implementation)
PLATFORM_SRCS = ../iql_ext/iql_platform.c

IMGUI_OBJS = $(patsubst %.cpp,$(BUILD)/%.o,$(IMGUI_SRCS))
APP_OBJS = $(patsubst %.cpp,$(BUILD)/%.o,$(APP_SRCS))
IQL_OBJS = $(patsubst $(IQL_DIR)/%.c,$(BUILD)/iql_%.o,$(IQL_SRCS))
PLATFORM_OBJS = $(BUILD)/iql_platform.o

ALL_OBJS = $(IMGUI_OBJS) $(APP_OBJS) $(IQL_OBJS) $(PLATFORM_OBJS)

all: $(TARGET)

$(TARGET): $(ALL_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(ALL_OBJS) $(LDFLAGS)

# C++ sources
$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# iQL C sources
$(BUILD)/iql_%.o: $(IQL_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Platform stub
$(BUILD)/iql_platform.o: ../iql_ext/iql_platform.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET)

.PHONY: all clean
