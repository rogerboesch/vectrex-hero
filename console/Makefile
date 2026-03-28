CC = cc
CFLAGS = -O2 -Wall -Wextra -std=c99
TARGET = hero
SRCS = main.c player.c enemies.c levels.c render.c terminal.c
OBJS = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c game.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: clean
