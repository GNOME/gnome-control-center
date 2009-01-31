bin_PROGRAMS = gnome-typing-monitor

gnome_typing_monitor_SOURCES =			\
	main.c					\
	drwright.c				\
	drwright.h				\
	drw-break-window.c			\
	drw-break-window.h			\
	drw-monitor.c				\
	drw-monitor.h				\
	drw-utils.c				\
	drw-utils.h				\
	drw-selection.c				\
	drw-selection.h

gnome_typing_monitor_CPPFLAGS = \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DIMAGEDIR=\"$(pkgdatadir)/pixmaps\"		\
	$(AM_CPPFLAGS)
gnome_typing_monitor_CFLAGS = \
	@TYPING_CFLAGS@	\
	$(AM_CFLAGS)

gnome_typing_monitor_LDADD = @TYPING_LIBS@ @SCREENSAVER_LIBS@

if HAVE_LIBCANBERRA_GTK
  gnome_typing_monitor_CFLAGS += -DHAVE_CANBERRA_GTK @LIBCANBERRA_GTK_CFLAGS@ 
  gnome_typing_monitor_LDADD += @LIBCANBERRA_GTK_LIBS@
endif

imagedir = $(pkgdatadir)/pixmaps
dist_image_DATA = bar.png bar-red.png bar-green.png bar-disabled.png ocean-stripes.png

# Themeable application icon
icondir = $(datadir)/icons/hicolor/48x48/apps
dist_icon_DATA = typing-monitor.png
svgicondir = $(datadir)/icons/hicolor/scalable/apps
dist_svgicon_DATA = typing-monitor.svg
gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor
install-data-hook: update-icon-cache
uninstall-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After (un)install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi
