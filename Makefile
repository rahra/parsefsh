CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lm

parsefsh: parsefsh.o

parsefsh.o: parsefsh.c parsefsh.h

clean:
	rm -f *.o parsefsh

.PHONY: clean

