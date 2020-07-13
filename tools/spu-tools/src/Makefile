ARCHIV := spu-tools.tar.gz

CC = gcc
CFLAGS = -g -Wall
PREFIX = $(DESTDIR)/usr

objs = ctx-info.o spu-info.o proc-info.o general-info.o spu-top.o spu-ps.o
target = spu-top spu-ps
all: $(target) man

ctx-info.o: ctx-info.c spu-tools.h
spu-info.o: spu-info.c spu-tools.h
proc-info.o: proc-info.c spu-tools.h
general-info.o: general-info.c spu-tools.h
spu-top.o: spu-top.c spu-tools.h
spu-ps.o: spu-ps.c spu-tools.h

spu-top: ctx-info.o spu-info.o proc-info.o general-info.o spu-top.o
	$(CC) $(CFLAGS) -lncurses ctx-info.o spu-info.o proc-info.o general-info.o spu-top.o -o spu-top

spu-ps: ctx-info.o spu-ps.o
	$(CC) $(CFLAGS) ctx-info.o spu-ps.o -o spu-ps

clean:
	@rm -Rf *.o *~ $(objs) $(target) spu-top.1 spu-ps.1
	@rm -f .rpmmacros

man: spu-ps spu-top
	help2man --no-info ./spu-top -o spu-top.1
	help2man --no-info ./spu-ps -o spu-ps.1

install:
	echo Installing at $(PREFIX)
	mkdir -p $(PREFIX)/bin
	cp spu-top spu-ps $(PREFIX)/bin
	mkdir -p $(PREFIX)/share/man/man1
	cp spu-top.1 spu-ps.1 $(PREFIX)/share/man/man1/
