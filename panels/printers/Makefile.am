cappletname = printers

uidir = $(pkgdatadir)/ui/printers
dist_ui_DATA = \
	new-printer-dialog.ui	\
	ppd-selection-dialog.ui	\
	options-dialog.ui	\
	jobs-dialog.ui		\
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
	printers-module.c		\
	pp-new-printer.c		\
	pp-new-printer.h		\
	pp-maintenance-command.c	\
	pp-maintenance-command.h	\
	pp-host.c			\
	pp-host.h			\
	pp-cups.c			\
	pp-cups.h			\
	pp-utils.c			\
	pp-utils.h			\
	pp-ppd-option-widget.c		\
	pp-ppd-option-widget.h		\
	pp-ipp-option-widget.c		\
	pp-ipp-option-widget.h		\
	pp-new-printer-dialog.c		\
	pp-new-printer-dialog.h		\
	pp-ppd-selection-dialog.c	\
	pp-ppd-selection-dialog.h	\
	pp-options-dialog.c		\
	pp-options-dialog.h		\
	pp-jobs-dialog.c		\
	pp-jobs-dialog.h		\
	cc-printers-panel.c		\
	cc-printers-panel.h

libprinters_la_LIBADD = $(PRINTERS_PANEL_LIBS) $(PANEL_LIBS) $(CUPS_LIBS)
libprinters_la_LDFLAGS = $(PANEL_LDFLAGS)

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-printers-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk
