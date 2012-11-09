cappletname = network

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(NETWORK_PANEL_CFLAGS)				\
	$(NETWORK_MANAGER_CFLAGS)			\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	$(NULL)

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libnetwork.la

libnetwork_la_SOURCES =					\
	network-module.c				\
	panel-common.c					\
	panel-common.h					\
	net-object.c					\
	net-object.h					\
	net-device.c					\
	net-device.h					\
	net-device-wifi.c				\
	net-device-wifi.h				\
	net-device-wired.c				\
	net-device-wired.h				\
	net-device-mobile.c				\
	net-device-mobile.h				\
	net-vpn.c					\
	net-vpn.h					\
	net-proxy.c					\
	net-proxy.h					\
	panel-cell-renderer-mode.c			\
	panel-cell-renderer-mode.h			\
	panel-cell-renderer-security.c			\
	panel-cell-renderer-security.h			\
	panel-cell-renderer-signal.c			\
	panel-cell-renderer-signal.h			\
	panel-cell-renderer-separator.c			\
	panel-cell-renderer-separator.h			\
	panel-cell-renderer-text.c			\
	panel-cell-renderer-text.h			\
	panel-cell-renderer-pixbuf.c			\
	panel-cell-renderer-pixbuf.h			\
	network-dialogs.c				\
	network-dialogs.h				\
	cc-network-panel.c				\
	cc-network-panel.h				\
	rfkill-glib.c					\
	rfkill-glib.h					\
	rfkill.h

libnetwork_la_LIBADD = $(PANEL_LIBS) $(NETWORK_PANEL_LIBS) $(NETWORK_MANAGER_LIBS)
libnetwork_la_LDFLAGS = $(PANEL_LDFLAGS)

uidir = $(pkgdatadir)/ui
dist_ui_DATA =						\
	network-proxy.ui				\
	network-vpn.ui					\
	network-wifi.ui					\
	network-wired.ui				\
	network-mobile.ui				\
	network.ui

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-network-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk
