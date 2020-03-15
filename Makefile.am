
SUBDIRS = src po

applicationsdir = $(datarootdir)/applications
dist_applications_DATA = gpicann.desktop

iconsdir = $(datarootdir)/icons/hicolor
iconsscalabledir = $(iconsdir)/scalable/apps
dist_iconsscalable_DATA = gpicann.svg

install-data-local:
	for size in 16 20 22 24 32 36 40 48 64 72 96 128 192 256 384 512; do \
	  convert -background none -size $${size}x$${size} $(srcdir)/gpicann.svg gpicann.png && \
	  $(MKDIR_P) "$(DESTDIR)$(iconsdir)/$${size}x$${size}/apps" && \
	  $(INSTALL_DATA) gpicann.png "$(DESTDIR)$(iconsdir)/$${size}x$${size}/apps"; \
	done

uninstall-local:
	for size in 16 20 22 24 32 36 40 48 64 72 96 128 192 256 384 512; do \
	  rm -f "$(DESTDIR)$(iconsdir)/$${size}x$${size}/apps/gpicann.png"; \
	done
