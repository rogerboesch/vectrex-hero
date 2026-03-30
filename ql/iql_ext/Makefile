# iql_ext — Build iQL emulator as CPython extension module
#
# Compiles the iQL emulator core (from the iQL project) plus our
# platform stubs and Python wrapper into _iql.cpython-*.so
#
# Usage: make
# Clean: make clean

IQL_DIR ?= /Users/roger/projects/iQL

# Python build config
PY_INCLUDES = $(shell python3-config --includes)
PY_LDFLAGS  = $(shell python3-config --ldflags)
PY_EXT      = $(shell python3 -c "import sysconfig; print(sysconfig.get_config_var('EXT_SUFFIX'))")

TARGET = _iql$(PY_EXT)
BUILD  = build

CC     = cc
CFLAGS = -O2 -fPIC -std=gnu89 -w -Wno-error \
         -Wno-int-conversion -Wno-incompatible-pointer-types \
         -DMOUSE=1 -DHAS_STPCPY \
         $(PY_INCLUDES) \
         -I$(IQL_DIR)/QLemulator/QL \
         -I$(IQL_DIR)/QLemulator/base \
         -I$(IQL_DIR)/QLemulator/platform \
         -I$(IQL_DIR)/QLemulator/apple \
         -DUSE_BUILTIN_EXPECT

LDFLAGS = -shared -undefined dynamic_lookup -lpthread

# iQL emulator core sources (from iQL project, excluding apple/ ObjC files)
IQL_QL_SRCS = \
	$(IQL_DIR)/QLemulator/QL/QL_basext.c \
	$(IQL_DIR)/QLemulator/QL/QL_boot.c \
	$(IQL_DIR)/QLemulator/QL/QL_cconv.c \
	$(IQL_DIR)/QLemulator/QL/QL_config.c \
	$(IQL_DIR)/QLemulator/QL/QL_disk.c \
	$(IQL_DIR)/QLemulator/QL/QL_driver.c \
	$(IQL_DIR)/QLemulator/QL/QL_emulator.c \
	$(IQL_DIR)/QLemulator/QL/QL_files.c \
	$(IQL_DIR)/QLemulator/QL/QL_hardware.c \
	$(IQL_DIR)/QLemulator/QL/QL_ip.c \
	$(IQL_DIR)/QLemulator/QL/QL_mouse.c \
	$(IQL_DIR)/QLemulator/QL/QL_poll.c \
	$(IQL_DIR)/QLemulator/QL/QL_screen.c \
	$(IQL_DIR)/QLemulator/QL/QL_serial.c \
	$(IQL_DIR)/QLemulator/QL/QL_serio.c \
	$(IQL_DIR)/QLemulator/QL/QL_sound.c \
	$(IQL_DIR)/QLemulator/QL/QL_traps.c \
	$(IQL_DIR)/QLemulator/QL/QL_vfs.c

IQL_BASE_SRCS = \
	$(IQL_DIR)/QLemulator/base/base_bdi.c \
	$(IQL_DIR)/QLemulator/base/base_cfg.c \
	$(IQL_DIR)/QLemulator/base/base_dummies.c \
	$(IQL_DIR)/QLemulator/base/base_general.c \
	$(IQL_DIR)/QLemulator/base/base_iexl_general.c \
	$(IQL_DIR)/QLemulator/base/base_init.c \
	$(IQL_DIR)/QLemulator/base/base_instructions_ao.c \
	$(IQL_DIR)/QLemulator/base/base_instructions_pz.c \
	$(IQL_DIR)/QLemulator/base/base_memaccess.c \
	$(IQL_DIR)/QLemulator/base/base_pty.c \
	$(IQL_DIR)/QLemulator/base/base_qmtrap.c \
	$(IQL_DIR)/QLemulator/base/base_script.c \
	$(IQL_DIR)/QLemulator/base/base_trace.c \
	$(IQL_DIR)/QLemulator/base/base_util.c \
	$(IQL_DIR)/QLemulator/base/base_uxfile.c \
	$(IQL_DIR)/QLemulator/base/base_version.c \
	$(IQL_DIR)/QLemulator/base/base_vm.c \
	$(IQL_DIR)/QLemulator/base/base_xc68.c \
	$(IQL_DIR)/QLemulator/base/base_xcodes.c

IQL_PLATFORM_SRCS = \
	$(IQL_DIR)/QLemulator/platform/rb_screen.c \
	$(IQL_DIR)/QLemulator/platform/rb_settings.c \
	$(IQL_DIR)/QLemulator/platform/rb_virtual_keys.c

# Our files (platform stubs + Python wrapper)
LOCAL_SRCS = iql_platform.c _iqlmodule.c

ALL_SRCS = $(IQL_QL_SRCS) $(IQL_BASE_SRCS) $(IQL_PLATFORM_SRCS) $(LOCAL_SRCS)

# Object files go into build/
IQL_OBJS = $(patsubst $(IQL_DIR)/QLemulator/%.c,$(BUILD)/iql_%.o,$(IQL_QL_SRCS) $(IQL_BASE_SRCS) $(IQL_PLATFORM_SRCS))
LOCAL_OBJS = $(patsubst %.c,$(BUILD)/%.o,$(LOCAL_SRCS))
ALL_OBJS = $(IQL_OBJS) $(LOCAL_OBJS)

all: $(TARGET)

$(BUILD):
	mkdir -p $(BUILD)

# Link shared library
$(TARGET): $(ALL_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(ALL_OBJS)

# Compile iQL sources → build/iql_*.o
$(BUILD)/iql_%.o: $(IQL_DIR)/QLemulator/%.c | $(BUILD)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile local sources → build/*.o
$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET)

.PHONY: all clean
