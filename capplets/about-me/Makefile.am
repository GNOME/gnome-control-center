bin_PROGRAMS = gnome-about-me

pixmapdir = $(GNOMECC_PIXMAPS_DIR)
pixmap_DATA = \
	gnome-about-me.png

gnome_about_me_LDADD = $(GNOMECC_CAPPLETS_LIBS) $(LIBEBOOK_LIBS) -lutil
gnome_about_me_SOURCES = 		\
	e-util-marshal.c				\
	e-util-marshal.h				\
	gnome-about-me-password.c	\
	e-image-chooser.c				\
	gnome-about-me.c

e-util-marshal.h: e-util-marshal.list
	( @GLIB_GENMARSHAL@ --prefix=e_util_marshal $(srcdir)/e-util-marshal.list \
	 							--header > util-marshal.tmp \
			&& mv util-marshal.tmp e-util-marshal.h ) \
		|| ( rm -f util-marshal.tmp && exit 1 )

e-util-marshal.c: e-util-marshal.h
	( @GLIB_GENMARSHAL@ --prefix=e_util_marshal $(srcdir)/e-util-marshal.list \
	 							--body > util-marshal.tmp \
			&& mv util-marshal.tmp e-util-marshal.c ) \
		|| ( rm -f util-marshal.tmp && exit 1 )

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
Desktop_in_files = gnome-about-me.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = gnome-about-me.glade

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS) $(LIBEBOOK_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = e-util-marshal.list $(Glade_DATA) $(Desktop_in_files) $(pixmap_DATA)
