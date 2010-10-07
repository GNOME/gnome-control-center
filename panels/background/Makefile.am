# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = background

uidir = $(pkgdatadir)/ui/background
dist_ui_DATA = \
	background.ui \
	display-base.png \
	display-overlay.png


INCLUDES =						\
	$(PANEL_CFLAGS)					\
	$(GNOMECC_CAPPLETS_CFLAGS)			\
	$(XML_CFLAGS)					\
	$(GNOME_DESKTOP_CFLAGS)				\
	$(DBUS_CFLAGS)					\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-DDATADIR="\"$(uidir)\""			\
	-DGNOME_DESKTOP_USE_UNSTABLE_API		\
	$(NULL)

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libbackground.la

libbackground_la_SOURCES =		\
	background-module.c	\
	cc-background-panel.c	\
	cc-background-panel.h	\
	bg-source.c		\
	bg-source.h		\
	bg-pictures-source.c			\
	bg-pictures-source.h			\
	bg-wallpapers-source.c			\
	bg-wallpapers-source.h			\
	bg-colors-source.c			\
	bg-colors-source.h			\
	gnome-wp-info.c  gnome-wp-item.c  gnome-wp-xml.c \
	gnome-wp-info.h  gnome-wp-item.h  gnome-wp-xml.h


libbackground_la_LIBADD = \
	$(PANEL_LIBS) \
	$(DBUS_LIBS) \
	$(GNOME_DESKTOP_LIBS) \
	$(XML_LIBS)

libbackground_la_LDFLAGS = $(PANEL_LDFLAGS)

if WITH_LIBSOCIALWEB
libbackground_la_SOURCES += bg-flickr-source.c bg-flickr-source.h
INCLUDES += $(SOCIALWEB_CFLAGS)
libbackground_la_LIBADD += $(SOCIALWEB_LIBS)
endif

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-background-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk
