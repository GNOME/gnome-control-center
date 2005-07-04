bin_PROGRAMS = gnome-about-me

pixmapdir = $(GNOMECC_PIXMAPS_DIR)
pixmap_DATA = 				\
	gnome-about-me.png		\
	gnome-about-me-bulb-on.png	\
	gnome-about-me-bulb-off.png

gnome_about_me_LDADD = $(GNOMECC_CAPPLETS_LIBS) $(LIBEBOOK_LIBS) -lutil
gnome_about_me_SOURCES = 		\
	eel-alert-dialog.c		\
	eel-alert-dialog.h		\
	eel-gtk-macros.h		\
	e-util-marshal.c		\
	e-util-marshal.h		\
	gnome-about-me-password.c	\
	gnome-about-me-password.h	\
	e-image-chooser.c		\
	e-image-chooser.h		\
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
