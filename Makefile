LDFLAGS=-lSDL -lncurses
CFLAGS=-Wall -std=c99
CC=gcc

all:		tracker

tracker:	main.o chip.o gui.o io.o
		gcc -o $@ $^ ${LDFLAGS}

%.o:		%.c stuff.h Makefile

clean:
	rm -rf main.o chip.o gui.o io.o
