# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = media

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libmedia.la

libmedia_la_SOURCES = \
	media-module.c \
	cc-media-panel.c \
	cc-media-panel.h

libmedia_la_LIBADD = \
	$(PANEL_LIBS) \
	$(CAPPLET_LIBS)

libmedia_la_LDFLAGS = $(PANEL_LDFLAGS)

@INTLTOOL_DESKTOP_RULE@

uidir = $(pkgdatadir)/ui
dist_ui_DATA = gnome-media-properties.ui

desktopdir = $(datadir)/applications
Desktop_in_files = gnome-media-panel.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES   = 					\
	$(GNOMECC_CAPPLETS_CFLAGS)		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"\
	-DGNOMECC_UI_DIR=\""$(uidir)"\"

CLEANFILES = \
	$(GNOMECC_CAPPLETS_CLEANFILES) \
	$(Desktop_in_files) \
	$(desktop_DATA)

-include $(top_srcdir)/git.mk
