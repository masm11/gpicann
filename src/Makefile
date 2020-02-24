gpicann: main.o rect.o
	cc -o gpicann main.o rect.o `pkg-config --libs gtk+-3.0`

%.o: %.c common.h shapes.h
	cc -Wall -c `pkg-config --cflags gtk+-3.0` $*.c
