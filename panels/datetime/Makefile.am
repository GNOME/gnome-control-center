# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = datetime

uidir = $(datadir)/ui/datetime
dist_ui_DATA = \
	datetime.ui \
	data/bg.png \
	data/cc.png \
	data/timezone_0.png \
	data/timezone_-10.png \
	data/timezone_10.png \
	data/timezone_10.5.png \
	data/timezone_-1.png \
	data/timezone_1.png \
	data/timezone_-11.png \
	data/timezone_11.png \
	data/timezone_11.5.png \
	data/timezone_12.png \
	data/timezone_12.75.png \
	data/timezone_13.png \
	data/timezone_-2.png \
	data/timezone_2.png \
	data/timezone_-3.png \
	data/timezone_3.png \
	data/timezone_-3.5.png \
	data/timezone_3.5.png \
	data/timezone_-4.png \
	data/timezone_4.png \
	data/timezone_-4.5.png \
	data/timezone_4.5.png \
	data/timezone_-5.png \
	data/timezone_5.png \
	data/timezone_-5.5.png \
	data/timezone_5.5.png \
	data/timezone_5.75.png \
	data/timezone_-6.png \
	data/timezone_6.png \
	data/timezone_6.5.png \
	data/timezone_-7.png \
	data/timezone_7.png \
	data/timezone_-8.png \
	data/timezone_8.png \
	data/timezone_-9.png \
	data/timezone_9.png \
	data/timezone_-9.5.png \
	data/timezone_9.5.png


INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(GNOMECC_CAPPLETS_CFLAGS)			\
	$(DBUS_CFLAGS)					\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-DDATADIR="\"$(uidir)\""			\
	$(NULL)

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libdate_time.la

libdate_time_la_SOURCES =		\
	datetime-module.c	\
	cc-datetime-panel.c	\
	cc-datetime-panel.h	\
	cc-timezone-map.c	\
	cc-timezone-map.h	\
	set-timezone.c		\
	set-timezone.h		\
	tz.c tz.h

libdate_time_la_LIBADD = $(PANEL_LIBS) $(DBUS_LIBS)
libdate_time_la_LDFLAGS = $(PANEL_LDFLAGS)

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-datetime-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk
