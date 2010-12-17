uidir = $(pkgdatadir)/ui/printers
dist_ui_DATA = \
	printers.ui

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(DBUS_CFLAGS)					\
	$(GNOMECC_CAPPLETS_CFLAGS)			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-DDATADIR="\"$(uidir)\""			\
	$(NULL)

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libprinters.la

libprinters_la_SOURCES =		\
	printers-module.c	\
	cc-printers-panel.c	\
	cc-printers-panel.h

libprinters_la_LIBADD = $(PANEL_LIBS) $(CUPS_LIBS) $(DBUS_LIBS)
libprinters_la_LDFLAGS = $(PANEL_LDFLAGS)

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-printers-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk
