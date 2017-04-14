CC = gcc
CFLAGS = -g 
#-D__APPLE__ -D__x86_64__

.SUFFIXES: .c

.c:
	make $*.o

OBJS = reallocarray.o lwan-array.o lwan-coro.o

all: test

libcoro.a: $(OBJS)
	ar rcv libcoro.a $(OBJS)
	- ranlib libcoro.a

test: libcoro.a
	$(CC) test/test.c -I. -L. -lcoro
	./a.out
	$(CC) test/permgen.c -I. -L. -lcoro
	./a.out

install: libcoro.a
	- cp libcoro.a /usr/local/lib
	- cp lwan-coro.h /usr/local/include

clean:
	rm -f *.o *~ a.out libcoro.a core
