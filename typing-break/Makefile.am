INCLUDES =						\
	@GNOME_CFLAGS@					\
	-DIMAGEDIR=\"$(GNOMECC_PIXMAPS_DIR)\"

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

gnome_typing_monitor_LDADD = @GNOME_LIBS@ @SCREENSAVER_LIBS@
gnome_typing_monitor_LDFLAGS = -export-dynamic

imagedir = $(GNOMECC_PIXMAPS_DIR)
image_DATA = stop.png bar.png bar-red.png bar-green.png bar-disabled.png ocean-stripes.png

# Themeable application icon
icondir = $(datadir)/icons/hicolor/48x48/apps
icon_DATA = typing-monitor.png
svgicondir = $(datadir)/icons/hicolor/scalable/apps
svgicon_DATA = typing-monitor.svg
gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor
install-data-hook:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

EXTRA_DIST = $(image_DATA)   \
             $(icon_DATA)    \
             $(svgicon_DATA)
