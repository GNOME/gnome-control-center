cappletname = universal-access

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(UNIVERSAL_ACCESS_PANEL_CFLAGS)		\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-I$(top_srcdir)/panels/common/			\
	$(NULL)

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libuniversal-access.la

libuniversal_access_la_SOURCES =		\
	universal-access-module.c		\
	cc-ua-panel.c				\
	cc-ua-panel.h				\
	gconf-property-editor.h			\
	gconf-property-editor.c			\
	cc-marshal.c				\
	cc-marshal.h				\
	zoom-options.c				\
	zoom-options.h

libuniversal_access_la_LIBADD = $(PANEL_LIBS) $(UNIVERSAL_ACCESS_PANEL_LIBS)
libuniversal_access_la_LDFLAGS = $(PANEL_LDFLAGS)

uidir = $(pkgdatadir)/ui
dist_ui_DATA = uap.ui zoom-options.ui


@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-universal-access-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk
