# tos2ql — Convert Atari ST TOS executable to self-relocating QDOS binary
#
# Build: make
# Usage: ./tos2ql input.tos output_ql

CC = cc
CFLAGS = -std=c11 -O2 -Wall

TARGET = tos2ql

all: $(TARGET)

$(TARGET): tos2ql.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
