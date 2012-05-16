# This is used in PANEL_CFLAGS
cappletname = region

INCLUDES =						\
	$(PANEL_CFLAGS)					\
	$(REGION_PANEL_CFLAGS)				\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	-I$(srcdir)/../common/				\
	$(NULL)

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libregion.la

libregion_la_SOURCES =	\
	region-module.c \
	cc-region-panel.c \
	cc-region-panel.h \
	gnome-region-panel-formats.c \
	gnome-region-panel-formats.h \
	gnome-region-panel-lang.c \
	gnome-region-panel-lang.h \
	gnome-region-panel-system.c \
	gnome-region-panel-system.h \
	gnome-region-panel-input.c \
	gnome-region-panel-input.h \
	$(NULL)

libregion_la_LIBADD = $(PANEL_LIBS) $(REGION_PANEL_LIBS) $(builddir)/../common/liblanguage.la

libregion_la_LDFLAGS = $(PANEL_LDFLAGS)

@INTLTOOL_DESKTOP_RULE@

uidir   = $(pkgdatadir)/ui
ui_DATA = \
	gnome-region-panel.ui \
	gnome-region-panel-input-chooser.ui \
	$(NULL)

desktopdir = $(datadir)/applications
Desktop_in_files = gnome-region-panel.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(Desktop_in_files) $(desktop_DATA)
EXTRA_DIST = $(ui_DATA)


-include $(top_srcdir)/git.mk
