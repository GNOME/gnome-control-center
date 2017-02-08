# This is used in PANEL_CFLAGS
cappletname = display

noinst_LTLIBRARIES = libdisplay.la

libdisplay_la_SOURCES =		\
	cc-display-panel.c	\
	cc-display-panel.h	\
	scrollarea.c		\
	scrollarea.h

libdisplay_la_LIBADD = $(PANEL_LIBS) $(DISPLAY_PANEL_LIBS) $(LIBM)

# You will need a recent intltool or the patch from this bug
# http://bugzilla.gnome.org/show_bug.cgi?id=462312
@INTLTOOL_POLICY_RULE@

@INTLTOOL_DESKTOP_RULE@

icons16dir = $(datadir)/icons/hicolor/16x16/apps
dist_icons16_DATA = icons/16x16/preferences-desktop-display.png
icons22dir = $(datadir)/icons/hicolor/22x22/apps
dist_icons22_DATA = icons/22x22/preferences-desktop-display.png
icons24dir = $(datadir)/icons/hicolor/24x24/apps
dist_icons24_DATA = icons/24x24/preferences-desktop-display.png
icons32dir = $(datadir)/icons/hicolor/32x32/apps
dist_icons32_DATA = icons/32x32/preferences-desktop-display.png
iconssvgdir = $(datadir)/icons/hicolor/scalable/apps
dist_iconssvg_DATA = icons/scalable/preferences-desktop-display.svg

desktopdir = $(datadir)/applications
Desktop_in_files = gnome-display-panel.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

AM_CPPFLAGS   = $(PANEL_CFLAGS) \
	     $(DISPLAY_PANEL_CFLAGS) \
	     -DGNOMELOCALEDIR="\"$(datadir)/locale\""

CLEANFILES = $(Desktop_in_files) $(desktop_DATA)

if MAINTAINER_MODE
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
endif

-include $(top_srcdir)/git.mk
