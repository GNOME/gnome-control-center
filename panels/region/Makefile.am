# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = region

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(GNOMECC_CAPPLETS_CFLAGS)			\
	$(LIBGNOMEKBD_CFLAGS)				\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-DGNOMECC_UI_DIR="\"$(uidir)\""
	$(NULL)

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libregion.la

libregion_la_SOURCES =	\
	region-module.c \
	cc-region-panel.c \
	cc-region-panel.h \
	gnome-region-panel-lang.c \
	gnome-region-panel-lang.h \
	gnome-region-panel-xkb.c \
	gnome-region-panel-xkbmc.c \
	gnome-region-panel-xkblt.c \
	gnome-region-panel-xkbltadd.c \
	gnome-region-panel-xkbot.c \
	gnome-region-panel-xkbpv.c \
	gnome-region-panel-xkb.h

libregion_la_LIBADD = $(PANEL_LIBS) $(LIBGNOMEKBD_LIBS) $(GNOMECC_CAPPLETS_LIBS)

libregion_la_LDFLAGS = $(PANEL_LDFLAGS)

@INTLTOOL_DESKTOP_RULE@

uidir   = $(pkgdatadir)/ui
ui_DATA = \
	gnome-region-panel.ui \
	gnome-region-panel-layout-chooser.ui \
	gnome-region-panel-model-chooser.ui \
	gnome-region-panel-options-dialog.ui

desktopdir = $(datadir)/applications
Desktop_in_files = gnome-region-panel.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(Desktop_in_files) $(desktop_DATA)
EXTRA_DIST = $(ui_DATA)


-include $(top_srcdir)/git.mk
