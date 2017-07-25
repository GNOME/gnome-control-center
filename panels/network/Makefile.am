cappletname = network

SUBDIRS = wireless-security connection-editor

AM_CPPFLAGS = 						\
	$(PANEL_CFLAGS)					\
	$(NETWORK_PANEL_CFLAGS)				\
	$(NETWORK_MANAGER_CFLAGS)			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-I$(srcdir)/wireless-security			\
	$(NULL)

noinst_LTLIBRARIES = libnetwork.la

BUILT_SOURCES =			\
	cc-network-resources.c	\
	cc-network-resources.h

libnetwork_la_SOURCES =					\
	$(BUILT_SOURCES)				\
	panel-common.c					\
	panel-common.h					\
	net-object.c					\
	net-object.h					\
	net-device.c					\
	net-device.h					\
	net-device-wifi.c				\
	net-device-wifi.h				\
	net-device-simple.c				\
	net-device-simple.h				\
	net-device-ethernet.c				\
	net-device-ethernet.h				\
	net-device-mobile.c				\
	net-device-mobile.h				\
	net-vpn.c					\
	net-vpn.h					\
	net-proxy.c					\
	net-proxy.h					\
	network-dialogs.c				\
	network-dialogs.h				\
	cc-network-panel.c				\
	cc-network-panel.h                              \
	cc-wifi-panel.c					\
	cc-wifi-panel.h

libnetwork_la_LIBADD = $(PANEL_LIBS) $(NETWORK_PANEL_LIBS) $(NETWORK_MANAGER_LIBS) $(builddir)/connection-editor/libconnection-editor.la

libnetwork_la_LDFLAGS = $(PANEL_LDFLAGS)

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/network.gresource.xml)
cc-network-resources.c: network.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_network $<
cc-network-resources.h: network.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_network $<

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-network-panel.desktop.in gnome-wifi-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = $(resource_files) network.gresource.xml meson.build

-include $(top_srcdir)/git.mk
