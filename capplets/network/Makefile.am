NULL =

# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = network

INCLUDES =						\
	$(GNOMECC_CAPPLETS_CFLAGS)			\
	-I$(top_srcdir)/libgnome-control-center-extension \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	$(NULL)

bin_PROGRAMS = gnome-network-properties

ccmodulesdir = $(EXTENSIONSDIR)
ccmodules_LTLIBRARIES = libnetwork.la

libnetwork_la_SOURCES =			\
	network-module.c		\
	cc-network-panel.h		\
	cc-network-panel.c		\
	cc-network-proxy-page.h		\
	cc-network-proxy-page.c		\
	$(NULL)

libnetwork_la_LDFLAGS =			\
	$(EXTENSION_LIBTOOL_FLAGS)	\
	$(NULL)

libnetwork_la_LIBADD =			\
	$(EXTENSION_LIBS)		\
	$(EXTENSION_COMMON_LIBS)	\
	$(NULL)

libnetwork_la_CFLAGS =			\
	$(EXTENSION_CFLAGS)		\
	$(EXTENSION_COMMON_CFLAGS)	\
	$(NULL)

libnetwork_la_LIBTOOLFLAGS = --tag=disable-static

gnome_network_properties_SOURCES = 	\
	gnome-network-properties.c	\
	$(NULL)

gnome_network_properties_LDADD =	\
	$(top_builddir)/libgnome-control-center-extension/libcommon.la \
	$(GNOMECC_CAPPLETS_LIBS)	\
	$(NULL)

@INTLTOOL_DESKTOP_RULE@

uidir = $(pkgdatadir)/ui
dist_ui_DATA = gnome-network-properties.ui

icons16dir = $(datadir)/icons/hicolor/16x16/apps
dist_icons16_DATA = icons/16x16/gnome-network-properties.png
icons22dir = $(datadir)/icons/hicolor/22x22/apps
dist_icons22_DATA = icons/22x22/gnome-network-properties.png
icons24dir = $(datadir)/icons/hicolor/24x24/apps
dist_icons24_DATA = icons/24x24/gnome-network-properties.png
icons32dir = $(datadir)/icons/hicolor/32x32/apps
dist_icons32_DATA = icons/32x32/gnome-network-properties.png
icons48dir = $(datadir)/icons/hicolor/48x48/apps
dist_icons48_DATA = icons/48x48/gnome-network-properties.png
iconssvgdir = $(datadir)/icons/hicolor/scalable/apps
dist_iconssvg_DATA = icons/scalable/gnome-network-properties.svg

desktopdir = $(datadir)/applications
desktop_in_files = gnome-network-properties.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor
uninstall-hook: update-icon-cache
install-data-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk
