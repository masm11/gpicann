OBJS = main.o handle.o rect.o arrow.o text.o mask.o settings.o icons.o
ICONS = arrow-bottom-left-thick blur file-export format-text image-edit rectangle-outline select

gpicann: $(OBJS)
	cc -o gpicann $(OBJS) `pkg-config --libs gtk+-3.0` -lm

%.o: %.c common.h shapes.h handle.h settings.h
	cc -O3 -Wall -c `pkg-config --cflags gtk+-3.0` $*.c

%.s: %.c
	cc -O3 -Wall -S `pkg-config --cflags gtk+-3.0` $*.c

icons.o: icons.c common.h icons.inc
	cc -O3 -Wall -c `pkg-config --cflags gtk+-3.0` $*.c

icons.inc: $(patsubst %, ../icons/%.svg, $(ICONS))
	./genicontable.sh $(ICONS) > $@.new
	mv $@.new $@

clean:
	-rm -f gpicann *.o *.s *~ *.inc
