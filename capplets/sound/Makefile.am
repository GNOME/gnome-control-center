bin_PROGRAMS = gnome2-sound-properties

gnome2_sound_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS) $(top_builddir)/libsounds/libsounds.a
gnome2_sound_properties_SOURCES = sound-properties-capplet.c

@INTLTOOL_DESKTOP_RULE@

Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = sound-properties.glade

iconsdir   = $(GNOMECC_ICONS_DIR)
icons_DATA = sound-capplet.png

desktop_iconsdir = $(datadir)/pixmaps
desktop_icons_DATA = sound-capplet.png

desktopdir = $(GNOMECC_DESKTOP_DIR)
Desktop_in_files = sound.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Glade_DATA) $(icons_DATA) $(Desktop_in_files)

