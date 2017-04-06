CC = gcc
CFLAGS = -c -g

.SUFFIXES: .c

.c:
	make $*.o

OBJS = reallocarray.o lwan-array.o lwan-coro.o

all: test

libcoro.a: $(OBJS)
	ar rcv libcoro.a $(OBJS)
	- ranlib libcoro.a

test: libcoro.a
	$(CC) -o test test.c -L. -lcoro
	./test

install: libcoro.a
	- cp libcoro.a /usr/local/lib
	- cp lwan-coro.h /usr/local/include

clean:
	rm -f *.o *~ test libcoro.a core
