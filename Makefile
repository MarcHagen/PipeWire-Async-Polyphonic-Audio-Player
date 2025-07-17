CC = gcc
CFLAGS = -Wall -Wextra -O2 -g $(shell pkg-config --cflags libpipewire-0.3 libspa-0.2)
LDLIBS = $(shell pkg-config --libs libpipewire-0.3 libspa-0.2) -lm

all: multichannel_player

multichannel_player: multichannel_player.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f multichannel_player

.PHONY: all clean
