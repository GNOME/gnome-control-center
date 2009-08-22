SUBDIRS = icons

# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = about-me

ui_files = gnome-about-me-dialog.ui gnome-about-me-password.ui gnome-about-me-fingerprint.ui
Desktop_in_files = gnome-about-me.desktop.in

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

MARSHALFILES = marshal.c marshal.h
BUILT_SOURCES = $(MARSHALFILES)

marshal.h: fprintd-marshal.list
	@GLIB_GENMARSHAL@ --prefix=fprintd_marshal $< --header > $@
marshal.c: fprintd-marshal.list
	@GLIB_GENMARSHAL@ --prefix=fprintd_marshal $< --body --header > $@

if BUILD_ABOUTME
bin_PROGRAMS = gnome-about-me

gnome_about_me_LDADD = $(GNOMECC_CAPPLETS_LIBS) $(LIBEBOOK_LIBS)
gnome_about_me_LDFLAGS = -export-dynamic

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

uidir   = $(pkgdatadir)/ui
ui_DATA = $(ui_files)

INCLUDES = \
	$(GNOMECC_CAPPLETS_CFLAGS) \
	$(LIBEBOOK_CFLAGS) \
	-DDATADIR="\"$(datadir)\"" \
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\"" \
	-DGNOMECC_UI_DIR="\"$(uidir)\"" \
	-DGNOMECC_PIXMAP_DIR="\"$(pkgdatadir)/pixmaps\"" \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""

endif # BUILD_ABOUTME

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(desktop_DATA) $(MARSHALFILES)
EXTRA_DIST = $(ui_files) fprintd-marshal.list

-include $(top_srcdir)/git.mk
