cappletname = printers

uidir = $(pkgdatadir)/ui/printers
dist_ui_DATA = \
	new-printer-dialog.ui	\
	printers.ui

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(PRINTERS_PANEL_CFLAGS)			\
	-I$(top_srcdir)/shell/				\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-DDATADIR="\"$(uidir)\""			\
	$(NULL)

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libprinters.la

libprinters_la_SOURCES =		\
	printers-module.c	\
	pp-utils.c	\
	pp-utils.h	\
	pp-new-printer-dialog.c	\
	pp-new-printer-dialog.h	\
	cc-printers-panel.c	\
	cc-printers-panel.h

libprinters_la_LIBADD = $(PRINTERS_PANEL_LIBS) $(PANEL_LIBS) $(CUPS_LIBS)
libprinters_la_LDFLAGS = $(PANEL_LDFLAGS)

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-printers-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk
