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
CC65_DIR ?= $(HOME)/retro-tools/cc65
CC  = $(CC65_DIR)/bin/cc65
CA  = $(CC65_DIR)/bin/ca65
LD  = $(CC65_DIR)/bin/ld65
AR  = $(CC65_DIR)/bin/ar65

# AppleCommander for disk images
AC ?= java -jar $(HOME)/retro-tools/AppleCommander-ac-1.8.0.jar

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

# --- Include paths ---
# cc65 needs to find its own includes
CC_FLAGS += -I $(CC65_DIR)/include
CA_FLAGS += -I $(CC65_DIR)/asminc

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
	$(LD) $(LD_FLAGS) -o $@ $(OBJS) $(CC65_DIR)/lib/$(TARGET).lib

# Create directories
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

# Disk image (ProDOS)
disk: $(BINDIR)/$(PROGRAM).bin
	cp $(HOME)/retro-tools/prodos/PRODOS.dsk $(BINDIR)/$(PROGRAM).dsk 2>/dev/null || \
	$(AC) -pro140 $(BINDIR)/$(PROGRAM).dsk RESCUE
	$(AC) -p $(BINDIR)/$(PROGRAM).dsk RESCUE.SYSTEM SYS < $(BINDIR)/$(PROGRAM).bin

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
