# C64 R.E.S.C.U.E. — Makefile for cc65
#
# Build: make
# Run:   x64sc rescue.prg  (VICE emulator)

CC65    ?= $(HOME)/retro-tools/cc65
CC       = $(CC65)/bin/cc65
CA       = $(CC65)/bin/ca65
LD       = $(CC65)/bin/ld65
CL       = $(CC65)/bin/cl65

# Try system cc65 first
ifeq ($(wildcard $(CC)),)
CC       = cc65
CA       = ca65
LD       = ld65
CL       = cl65
endif

TARGET   = rescue.prg
SRCDIR   = src

CFLAGS   = -t c64 -O --static-locals
ASFLAGS  = -t c64
LDFLAGS  = -C c64.cfg

SRCS     = $(SRCDIR)/main.c $(SRCDIR)/player.c $(SRCDIR)/enemies.c \
           $(SRCDIR)/levels.c $(SRCDIR)/render.c $(SRCDIR)/tiles.c \
           $(SRCDIR)/level_data.c $(SRCDIR)/charset.c
OBJS     = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CL) $(LDFLAGS) -o $@ $(OBJS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c $(SRCDIR)/game.h $(SRCDIR)/tiles.h
	$(CL) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(SRCDIR)/*.o

.PHONY: clean
