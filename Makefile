CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LDLIBS = -lpipewire-0.3 -lm

all: multichannel_player

multichannel_player: multichannel_player.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f multichannel_player

.PHONY: all clean
