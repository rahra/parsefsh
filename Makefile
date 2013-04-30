CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lm

parsefsh: parsefsh.o fshfunc.o

parsefsh.o: parsefsh.c fshfunc.h

fshfunc.o: fshfunc.c fshfunc.h

clean:
	rm -f *.o parsefsh

.PHONY: clean

