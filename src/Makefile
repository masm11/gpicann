OBJS = main.o rect.o arrow.o text.o mask.o

gpicann: $(OBJS)
	cc -o gpicann $(OBJS) `pkg-config --libs gtk+-3.0` -lm

%.o: %.c common.h shapes.h
	cc -Wall -c `pkg-config --cflags gtk+-3.0` $*.c
