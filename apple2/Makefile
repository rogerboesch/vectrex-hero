# Makefile -- Apple IIe R.E.S.C.U.E. build system
#
# Requires: cc65 toolchain (cc65, ca65, ld65)
# Optional: AppleCommander (ac.jar) for disk image creation
#
# Usage:
#   make          Build rescue.bin
#   make disk     Build rescue.dsk (ProDOS disk image)
#   make clean    Remove build artifacts
#   make stats    Show binary size

# --- Toolchain ---
# Try system cc65 first, fall back to retro-tools
CC65_DIR ?= $(shell dirname $$(which cc65 2>/dev/null || echo $(HOME)/retro-tools/cc65/bin/cc65))/..
CC  = cc65
CA  = ca65
LD  = ld65

# cc65 library path
CC65_LIB = $(shell $(CC) --print-target-path 2>/dev/null || echo $(CC65_DIR)/share/cc65)/lib

# AppleCommander for disk images (bundled in biplane project)
AC_JAR ?= $(HOME)/projects/biplane/platform_apple2/tools/AppleCommander-ac.jar
AC = java -jar $(AC_JAR)

# --- Target ---
TARGET   = apple2enh
CFG      = apple2.cfg
PROGRAM  = rescue

# --- Directories ---
SRCDIR   = src
OBJDIR   = obj
BINDIR   = bin

# --- Source files ---
C_SRCS   = $(SRCDIR)/main.c \
           $(SRCDIR)/player.c \
           $(SRCDIR)/enemies.c \
           $(SRCDIR)/levels.c \
           $(SRCDIR)/render.c \
           $(SRCDIR)/sprites.c

ASM_SRCS = $(SRCDIR)/a2_hw.s \
           $(SRCDIR)/blit.s

# --- Object files ---
C_OBJS   = $(C_SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
ASM_OBJS = $(ASM_SRCS:$(SRCDIR)/%.s=$(OBJDIR)/%.o)
OBJS     = $(C_OBJS) $(ASM_OBJS)

# --- Flags ---
CC_FLAGS  = -t $(TARGET) -O --add-source
CA_FLAGS  = -t $(TARGET)
LD_FLAGS  = -C $(CFG) -m $(BINDIR)/$(PROGRAM).map

# --- Rules ---

.PHONY: all clean disk stats

all: $(BINDIR)/$(PROGRAM).bin

# Compile C -> assembly -> object
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CC_FLAGS) -o $(OBJDIR)/$*.s $<
	$(CA) $(CA_FLAGS) -o $@ $(OBJDIR)/$*.s

# Assemble .s -> object
$(OBJDIR)/%.o: $(SRCDIR)/%.s | $(OBJDIR)
	$(CA) $(CA_FLAGS) -o $@ $<

# Link
$(BINDIR)/$(PROGRAM).bin: $(OBJS) $(CFG) | $(BINDIR)
	$(LD) $(LD_FLAGS) -o $@ $(OBJS) $(CC65_LIB)/$(TARGET).lib

# Create directories
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# Disk image (ProDOS .po format for Virtual ][ and other emulators)
# Creates a ProDOS disk with RESCUE.SYSTEM as SYS file.
# To boot: insert a ProDOS system disk first, then swap to this disk,
# or provide PRODOS file via: make disk PRODOS=/path/to/PRODOS
disk: $(BINDIR)/$(PROGRAM).bin
	rm -f $(BINDIR)/$(PROGRAM).po
	$(AC) -pro140 $(BINDIR)/$(PROGRAM).po RESCUE
	$(AC) -p $(BINDIR)/$(PROGRAM).po RESCUE.SYSTEM SYS '$$0803' < $(BINDIR)/$(PROGRAM).bin
ifdef PRODOS
	@echo "Adding ProDOS from $(PRODOS)..."
	$(AC) -p $(BINDIR)/$(PROGRAM).po PRODOS SYS '$$2000' < $(PRODOS)
endif
	@echo ""
	@echo "Disk image: $(BINDIR)/$(PROGRAM).po"
	$(AC) -ll $(BINDIR)/$(PROGRAM).po
	@echo ""
ifndef PRODOS
	@echo "NOTE: No PRODOS file on disk -- not self-booting."
	@echo "To make bootable: make disk PRODOS=/path/to/PRODOS"
	@echo "Or in Virtual ][: boot a ProDOS disk, then:"
	@echo "  1. Swap rescue.po into Slot 6 Drive 1"
	@echo "  2. Type: PREFIX /RESCUE"
	@echo "  3. Type: -RESCUE.SYSTEM"
else
	@echo "Bootable ProDOS disk ready!"
	@echo "In Virtual ][: insert rescue.po in Slot 6 Drive 1, reset."
endif

clean:
	rm -rf $(OBJDIR) $(BINDIR)

stats: $(BINDIR)/$(PROGRAM).bin
	@echo "Binary size:"
	@ls -la $(BINDIR)/$(PROGRAM).bin
	@echo ""
	@echo "Object sizes:"
	@ls -la $(OBJDIR)/*.o 2>/dev/null || true
	@echo ""
	@echo "Map file: $(BINDIR)/$(PROGRAM).map"
