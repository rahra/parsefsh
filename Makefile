CC = gcc
CFLAGS = -Wall -Wextra -g -std=gnu99 -DHAVE_VLOG
LDFLAGS = -lm
VERSION = 1.0-1514
DISTDIR = parsefsh-$(VERSION)
DESTDIR = /usr/local/bin
DISTFILES = LICENSE Makefile admfunc.h fshfunc.c fshfunc.h parseadm.c parsefsh.c projection.c splitimg.c projection.h
TARGETS = parsefsh parseadm splitimg

all: $(TARGETS)

parsefsh: parsefsh.o fshfunc.o projection.o

parsefsh.o: parsefsh.c fshfunc.h

fshfunc.o: fshfunc.c fshfunc.h

projection.o: projection.c projection.h

parseadm.o: parseadm.c admfunc.h

parseadm: parseadm.o projection.o

splitimg.o: splitimg.c admfunc.h

splitimg: splitimg.o

dist:
	rm -rf $(DISTDIR)
	mkdir $(DISTDIR)
	cp $(DISTFILES) $(DISTDIR)
	tar cvfj $(DISTDIR).tbz2 $(DISTDIR)

install:
	install $(TARGETS) $(DESTDIR)

clean:
	rm -f *.o $(TARGETS)

.PHONY: clean dist install

