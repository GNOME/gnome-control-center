cappletname = privacy

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(PRIVACY_PANEL_CFLAGS)				\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-DDATADIR="\"$(datadir)\""			\
	-I$(top_srcdir)/panels/common/			\
	$(NULL)

noinst_LTLIBRARIES = libprivacy.la

libprivacy_la_SOURCES =		\
	cc-privacy-panel.c	\
	cc-privacy-panel.h

libprivacy_la_LIBADD = $(PANEL_LIBS) $(PRIVACY_PANEL_LIBS)

uidir = $(pkgdatadir)/ui
dist_ui_DATA = 		\
	privacy.ui

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-privacy-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk
