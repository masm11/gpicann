gpicann: main.o
	cc -o gpicann main.o `pkg-config --libs gtk+-3.0`

main.o: main.c
	cc -c `pkg-config --cflags gtk+-3.0` main.c
