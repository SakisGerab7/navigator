CC=gcc
CFLAGS=-Wall -g -fsanitize=address
LIBS=-lncurses -lm

navigator: navigator.c
	$(CC) $(CFLAGS) $^ -o $@ $(LIBS)