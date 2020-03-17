#

bin_PROGRAMS = gpicann
gpicann_SOURCES = arrow.c handle.c icons.c main.c mask.c rect.c settings.c text.c state_mgmt.c tcos.c \
                  common.h handle.h settings.h shapes.h gettext.h tcos.h

nodist_gpicann_SOURCES = icons.inc
BUILT_SOURCES = icons.inc
CLEANFILES = icons.inc
icons.inc: $(wildcard $(top_srcdir)/icons/*.svg)
	$(srcdir)/genicontable.sh $(wildcard $(top_srcdir)/icons/*.svg) > $@.new
	mv $@.new $@

CFLAGS = -g -O3
AM_CFLAGS = $(GTK_CFLAGS) -DLOCALEDIR=\""$(datadir)/locale"\" -DENABLE_NLS=1
gpicann_LDADD = $(GTK_LIBS)

%.s: %.c
	$(COMPILE) -S -o $*.s $*.c
