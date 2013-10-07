CC = gcc
CFLAGS = -Wall -Wextra -g -std=gnu99 -DHAVE_VLOG
LDFLAGS = -lm
VERSION = 1.0-1505
DISTDIR = parsefsh-$(VERSION)
DESTDIR = /usr/local/bin

all: parsefsh parseadm

parsefsh: parsefsh.o fshfunc.o projection.o

parsefsh.o: parsefsh.c fshfunc.h

fshfunc.o: fshfunc.c fshfunc.h

projection.o: projection.c projection.h

parseadm.o: parseadm.c admfunc.h

parseadm: parseadm.o projection.o

dist:
	rm -rf $(DISTDIR)
	mkdir $(DISTDIR)
	cp parsefsh.c fshfunc.c fshfunc.h Makefile LICENSE $(DISTDIR)
	tar cvfj $(DISTDIR).tbz2 $(DISTDIR)

install:
	install parsefsh parseadm $(DESTDIR)

clean:
	rm -f *.o parsefsh

.PHONY: clean dist install

