bin_PROGRAMS = gnome-about-me

pixmapdir = $(GNOMECC_PIXMAPS_DIR)
pixmap_DATA = \
	gnome-about-me.png

gnome_about_me_LDADD = $(GNOMECC_CAPPLETS_LIBS) $(LIBEBOOK_LIBS) -lutil
gnome_about_me_SOURCES = 		\
	gnome-about-me-password.c	\
	e-util-marshal.c				\
	e-image-chooser.c				\
	gnome-about-me.c

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(GNOMECC_DESKTOP_DIR)
Desktop_in_files = gnome-about-me.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = gnome-about-me.glade

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS) $(LIBEBOOK_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Glade_DATA) $(Desktop_in_files) $(pixmap_DATA)
