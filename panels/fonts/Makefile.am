# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = fonts

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(GNOMECC_CAPPLETS_CFLAGS)			\
	$(FONT_CAPPLET_CFLAGS) 				\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	-DGNOMECC_PIXMAP_DIR="\"$(pixmapdir)\"" \
	$(NULL)


ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libfonts.la

libfonts_la_SOURCES = \
	fonts-module.c		\
	cc-fonts-panel.c	\
	cc-fonts-panel.h
libfonts_la_LIBADD = \
	$(PANEL_LIBS) \
	$(FONT_CAPPLET_LIBS)
libfonts_la_LDFLAGS = \
	$(PANEL_LDFLAGS) \
	$(FONT_CAPPLET_LDFLAGS)

@INTLTOOL_DESKTOP_RULE@

uidir = $(pkgdatadir)/ui
dist_ui_DATA = gnome-font-properties.ui

desktopdir = $(datadir)/applications
desktop_in_files = gnome-fonts-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(Desktop_in_files) $(desktop_DATA)
EXTRA_DIST = $(ui_DATA) $(pixmap_DATA)

-include $(top_srcdir)/git.mk
