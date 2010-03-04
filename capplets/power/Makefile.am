@INTLTOOL_DESKTOP_RULE@

cappletname = power

INCLUDES = 						\
	$(GNOMECC_CAPPLETS_CFLAGS)			\
	-I$(top_srcdir)/libgnome-control-center-extension	\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-DGNOMECC_UI_DIR="\"$(uidir)\""

ccmodulesdir = $(EXTENSIONSDIR)
ccmodules_LTLIBRARIES = libpower.la

libpower_la_SOURCES = \
	power-module.c \
	cc-power-panel.h cc-power-panel.c

libpower_la_CFLAGS = \
	$(EXTENSION_CFLAGS) \
	$(EXTENSION_COMMON_CFLAGS)

libpower_la_LDFLAGS = \
	$(EXTENSION_LIBTOOL_FLAGS)

libpower_la_LIBADD = \
	$(EXTENSION_LIBS) \
	$(EXTENSION_COMMON_LIBS)

uidir   = $(pkgdatadir)/ui
dist_ui_DATA = power.ui

desktopdir = $(datadir)/applications
desktop_in_files = power-properties.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk
