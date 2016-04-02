CC = gcc
CFLAGS = -Wall -Wextra -g -std=gnu99 -DHAVE_VLOG
LDFLAGS = -lm
VERSION = 1.0-g231
DISTDIR = parsefsh-$(VERSION)
DESTDIR = /usr/local/bin
DISTFILES = ../README.md ../LICENSE Makefile admfunc.h fshfunc.c fshfunc.h parsetrk.c parsefsh.c projection.c splitimg.c projection.h
TARGETS = parsefsh parsetrk splitimg

all: $(TARGETS)

parsefsh: parsefsh.o fshfunc.o projection.o

parsefsh.o: parsefsh.c fshfunc.h

fshfunc.o: fshfunc.c fshfunc.h

projection.o: projection.c projection.h

parsetrk.o: parsetrk.c admfunc.h

parsetrk: parsetrk.o projection.o

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

version:
	git log --oneline | wc -l

.PHONY: clean dist install version

