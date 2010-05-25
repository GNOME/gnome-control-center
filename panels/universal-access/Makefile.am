INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(GNOMECC_CAPPLETS_CFLAGS)			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	$(NULL)

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libuniversal-access.la

libuniversal_access_la_SOURCES =		\
	universal-access-module.c	\
	cc-ua-panel.c	\
	cc-ua-panel.h

libuniversal_access_la_LIBADD = $(PANEL_LIBS)
libuniversal_access_la_LDFLAGS = $(PANEL_LDFLAGS)

uidir = $(pkgdatadir)/ui
ui_DATA = uap.ui


@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-universal-access.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk
