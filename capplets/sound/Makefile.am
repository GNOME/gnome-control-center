bin_PROGRAMS = gnome-sound-properties

gnome_sound_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS) $(SOUND_CAPPLET_LIBS) $(top_builddir)/libsounds/libsounds.a
gnome_sound_properties_SOURCES = sound-properties-capplet.c

@INTLTOOL_DESKTOP_RULE@

Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = sound-properties.glade

desktop_iconsdir = $(GNOMECC_PIXMAPS_DIR)
desktop_icons_DATA = visual-bell.png

desktopdir = $(GNOMECC_DESKTOP_DIR)
Desktop_in_files = gnome-settings-sound.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS) $(SOUND_CAPPLET_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Glade_DATA) $(desktop_icons_DATA) $(Desktop_in_files)
