cappletname = power

SUBDIRS = icons

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(POWER_PANEL_CFLAGS)				\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-I$(srcdir)/../../shell/			\
	$(NULL)

noinst_LTLIBRARIES = libpower.la

libpower_la_SOURCES =		\
	cc-power-panel.c	\
	cc-power-panel.h

libpower_la_LIBADD = $(PANEL_LIBS) $(POWER_PANEL_LIBS)

if BUILD_BLUETOOTH
INCLUDES += $(BLUETOOTH_CFLAGS)
libpower_la_LIBADD += $(BLUETOOTH_LIBS)
endif

if BUILD_NETWORK
INCLUDES += $(NETWORK_MANAGER_CFLAGS)
libpower_la_LIBADD += $(NETWORK_MANAGER_LIBS)
endif

uidir = $(pkgdatadir)/ui
dist_ui_DATA = power.ui

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-power-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk
