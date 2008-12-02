# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = about-me

glade_files = gnome-about-me.glade gnome-about-me-fingerprint.glade
Desktop_in_files = gnome-about-me.desktop.in

pixmap_files = 				\
	gnome-about-me-lock.png		\
	gnome-about-me-lock-open.png

gnome_about_me_SOURCES =	 	\
	eel-alert-dialog.c		\
	eel-alert-dialog.h		\
	eel-gtk-macros.h		\
	gnome-about-me-password.c	\
	gnome-about-me-password.h	\
	e-image-chooser.c		\
	e-image-chooser.h		\
	gnome-about-me-fingerprint.c	\
	gnome-about-me-fingerprint.h	\
	$(MARSHALFILES)			\
	fingerprint-strings.h		\
	gnome-about-me.c

if BUILD_ABOUTME
bin_PROGRAMS = gnome-about-me

pixmapdir = $(pkgdatadir)/pixmaps
pixmap_DATA = $(pixmap_files)

gnome_about_me_LDADD = $(GNOMECC_CAPPLETS_LIBS) $(LIBEBOOK_LIBS) $(POLKIT_GNOME_LIBS)
gnome_about_me_LDFLAGS = -export-dynamic

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

gladedir   = $(pkgdatadir)/glade
glade_DATA = $(glade_files)

INCLUDES = \
	$(GNOMECC_CAPPLETS_CFLAGS) \
	$(LIBEBOOK_CFLAGS) \
	$(POLKIT_GNOME_CFLAGS) \
	-DDATADIR="\"$(datadir)\"" \
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\"" \
	-DGNOMECC_GLADE_DIR="\"$(gladedir)\"" \
	-DGNOMECC_PIXMAP_DIR="\"$(pixmapdir)\"" \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""

MARSHALFILES = marshal.c marshal.h
BUILT_SOURCES = $(MARSHALFILES)

marshal.h: $(srcdir)/fprintd-marshal.list
	( $(GLIB_GENMARSHAL) --prefix=fprintd_marshal $(srcdir)/fprintd-marshal.list --header > marshal.h )
marshal.c: marshal.h
	( $(GLIB_GENMARSHAL) --prefix=fprintd_marshal $(srcdir)/fprintd-marshal.list --body --header > marshal.c )


endif # BUILD_ABOUTME

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(desktop_DATA) $(MARSHALFILES)
EXTRA_DIST = $(glade_files) $(pixmap_files) fprintd-marshal.list
