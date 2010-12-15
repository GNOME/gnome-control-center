INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(GNOMECC_CAPPLETS_CFLAGS)			\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	$(NULL)

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libnetwork.la

libnetwork_la_SOURCES =					\
	network-module.c				\
	panel-common.c					\
	panel-common.h					\
	panel-cell-renderer-mode.c			\
	panel-cell-renderer-mode.h			\
	panel-cell-renderer-signal.c			\
	panel-cell-renderer-signal.h			\
	cc-network-panel.c				\
	cc-network-panel.h

libnetwork_la_LIBADD = $(PANEL_LIBS) $(CAPPLET_LIBS)
libnetwork_la_LDFLAGS = $(PANEL_LDFLAGS)

uidir = $(pkgdatadir)/ui
dist_ui_DATA = network.ui

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-network-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk
