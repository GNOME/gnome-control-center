# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = mouse

bin_PROGRAMS = gnome-mouse-properties

gnome_mouse_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS)
gnome_mouse_properties_SOURCES =	\
	gnome-mouse-properties.c	\
	gnome-mouse-accessibility.c	\
	gnome-mouse-accessibility.h

@INTLTOOL_DESKTOP_RULE@

pixmapdir  = $(pkgdatadir)/pixmaps
pixmap_DATA = \
	double-click-on.png		\
	double-click-off.png		\
	double-click-maybe.png

uidir   = $(pkgdatadir)/ui
ui_DATA = gnome-mouse-properties.ui

desktopdir = $(datadir)/applications
Desktop_in_files = gnome-settings-mouse.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES = \
	$(GNOMECC_CAPPLETS_CFLAGS) \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\"" \
	-DGNOMECC_UI_DIR="\"$(uidir)\""
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(Desktop_in_files) $(desktop_DATA)
EXTRA_DIST = $(ui_DATA) $(pixmap_DATA)

-include $(top_srcdir)/git.mk
