cappletname = location

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(LOCATION_PANEL_CFLAGS)			\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	$(NULL)

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = liblocation.la

liblocation_la_SOURCES =    \
	location-module.c   \
	cc-location-panel.c \
	cc-location-panel.h 

liblocation_la_LIBADD = $(PANEL_LIBS) $(LOCATION_PANEL_LIBS)
liblocation_la_LDFLAGS = $(PANEL_LDFLAGS)

uidir = $(pkgdatadir)/ui
dist_ui_DATA = location.ui

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-location-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk
