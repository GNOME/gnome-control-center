noinst_LIBRARIES = libaccessibility-keyboard.a

libaccessibility_keyboard_a_SOURCES =		\
    accessibility-keyboard.c			\
    accessibility-keyboard.h

# in case we need to forward port the sun accessX extension
#    AccessXcomm.c

bin_PROGRAMS = gnome-accessibility-keyboard-properties

gnome_accessibility_keyboard_properties_SOURCES = gnome-accessibility-keyboard-properties.c
gnome_accessibility_keyboard_properties_LDADD = \
	libaccessibility-keyboard.a $(GNOMECC_CAPPLETS_LIBS)

@INTLTOOL_DESKTOP_RULE@

pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
pixmap_DATA = \
	accessibility-keyboard-togglekey.png \
	accessibility-keyboard-mousekey.png \
	accessibility-keyboard-slowkey.png \
	accessibility-keyboard-stickykey.png \
	accessibility-keyboard-bouncekey.png

Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = gnome-accessibility-keyboard-properties.glade

iconsdir   = $(GNOMECC_ICONS_DIR)
icons_DATA = accessibility-keyboard-capplet.png

desktop_iconsdir = $(datadir)/pixmaps
desktop_icons_DATA = accessibility-keyboard-capplet.png

desktopdir = $(GNOMECC_DESKTOP_DIR)
Desktop_in_files = accessibility-keyboard.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Glade_DATA) $(icons_DATA) $(Desktop_in_files) $(pixmap_DATA)

