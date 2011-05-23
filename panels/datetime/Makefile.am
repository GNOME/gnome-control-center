# This is used in PANEL_CFLAGS
cappletname = datetime

SUBDIRS = po-timezones

icons16dir = $(datadir)/icons/hicolor/16x16/apps
dist_icons16_DATA = icons/16x16/preferences-system-time.png
icons22dir = $(datadir)/icons/hicolor/22x22/apps
dist_icons22_DATA = icons/22x22/preferences-system-time.png
icons32dir = $(datadir)/icons/hicolor/32x32/apps
dist_icons32_DATA = icons/32x32/preferences-system-time.png
icons48dir = $(datadir)/icons/hicolor/48x48/apps
dist_icons48_DATA = icons/48x48/preferences-system-time.png
icons256dir = $(datadir)/icons/hicolor/256x256/apps
dist_icons256_DATA = icons/256x256/preferences-system-time.png
iconsscalabledir = $(datadir)/icons/hicolor/scalable/apps
dist_iconsscalable_DATA = icons/scalable/preferences-system-time.svg

uidir = $(pkgdatadir)/ui/datetime
dist_ui_DATA = \
	datetime.ui \
	data/bg.png \
	data/cc.png \
	data/pin.png \
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
	data/timezone_14.png \
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
	data/timezone_8.75.png \
	data/timezone_-9.png \
	data/timezone_9.png \
	data/timezone_-9.5.png \
	data/timezone_9.5.png

tzdatadir = $(pkgdatadir)/datetime
dist_tzdata_DATA = backward

AM_CPPFLAGS =						\
	$(PANEL_CFLAGS)					\
	$(DATETIME_PANEL_CFLAGS)			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-DDATADIR="\"$(uidir)\""			\
	$(NULL)

noinst_PROGRAMS = test-timezone-gfx test-endianess test-timezone

test_timezone_SOURCES = test-timezone.c cc-timezone-map.h cc-timezone-map.c tz.c tz.h
test_timezone_LDADD = $(DATETIME_PANEL_LIBS)
test_timezone_CFLAGS = $(DATETIME_PANEL_CFLAGS)

test_timezone_gfx_SOURCES = test-timezone-gfx.c tz.c tz.h
test_timezone_gfx_LDADD = $(DATETIME_PANEL_LIBS)
test_timezone_gfx_CFLAGS = $(DATETIME_PANEL_CFLAGS)

test_endianess_SOURCES = test-endianess.c date-endian.c date-endian.h
test_endianess_LDADD = $(DATETIME_PANEL_LIBS)
test_endianess_CFLAGS = $(DATETIME_PANEL_CFLAGS)

all-local: check-local

check-local: test-timezone-gfx test-endianess test-timezone
	$(builddir)/test-timezone-gfx $(srcdir)/data
	$(builddir)/test-endianess
#	$(builddir)/test-timezone

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libdate_time.la

# FIXME add a way to regenerate the dtm.[ch]
# gdbus-codegen --generate-c-code dtm  --interface-prefix org.gnome.SettingsDaemon  /path/to/gnome-settings-daemon/plugins/datetime/gsd-datetime-mechanism.xml
# See also:
# https://bugzilla.gnome.org/show_bug.cgi?id=650875
# https://bugzilla.gnome.org/show_bug.cgi?id=650874

libdate_time_la_SOURCES =	\
	datetime-module.c	\
	cc-datetime-panel.c	\
	cc-datetime-panel.h	\
	cc-timezone-map.c	\
	cc-timezone-map.h	\
	dtm.c			\
	dtm.h			\
	date-endian.c		\
	date-endian.h		\
	tz.c tz.h

libdate_time_la_LIBADD = $(PANEL_LIBS) $(DATETIME_PANEL_LIBS)
libdate_time_la_LDFLAGS = $(PANEL_LDFLAGS)

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-datetime-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk

