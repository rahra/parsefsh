CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lm
VERSION = 1.0-1467
DISTDIR = parsefsh-$(VERSION)
DESTDIR = /usr/local/bin

parsefsh: parsefsh.o fshfunc.o

parsefsh.o: parsefsh.c fshfunc.h

fshfunc.o: fshfunc.c fshfunc.h

dist:
	rm -rf $(DISTDIR)
	mkdir $(DISTDIR)
	cp parsefsh.c fshfunc.c fshfunc.h Makefile LICENSE $(DISTDIR)
	tar cvfj $(DISTDIR).tbz2 $(DISTDIR)

install:
	install parsefsh $(DESTDIR)

clean:
	rm -f *.o parsefsh

.PHONY: clean dist install

