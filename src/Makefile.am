#

noinst_PROGRAMS = gentcos
gentcos_SOURCES = gentcos.c tcos.h

bin_PROGRAMS = gpicann
gpicann_SOURCES = arrow.c handle.c icons.c main.c mask.c rect.c settings.c text.c state_mgmt.c state_mgmt.h tcos.c \
                  common.h handle.h settings.h shapes.h gettext.h tcos.h

EXTRA_DIST = genicontable.sh

nodist_gpicann_SOURCES = icons.inc tcos.inc
BUILT_SOURCES = icons.inc tcos.inc
CLEANFILES = icons.inc tcos.inc

icons.inc: $(wildcard $(top_srcdir)/icons/*.svg)
	$(srcdir)/genicontable.sh $(wildcard $(top_srcdir)/icons/*.svg) > $@.new
	mv $@.new $@

tcos.inc: gentcos$(EXEEXT)
	./gentcos$(EXEEXT) > $@.new
	mv -f $@.new $@

AM_CFLAGS = $(GTK_CFLAGS) -DLOCALEDIR=\""$(datadir)/locale"\" -DENABLE_NLS=1
gpicann_LDADD = $(GTK_LIBS)

%.s: %.c
	$(COMPILE) -S -o $*.s $*.c
