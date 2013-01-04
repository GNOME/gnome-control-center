cappletname = network

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(NETWORK_PANEL_CFLAGS)				\
	$(NETWORK_MANAGER_CFLAGS)			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
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
	net-device-bond.c				\
	net-device-bond.h				\
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

resource_files = $(shell glib-compile-resources --generate-dependencies $(srcdir)/network.gresource.xml)
cc-network-resources.c: network.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_network $<
cc-network-resources.h: network.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_network $<

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-network-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = $(resource_files) network.gresource.xml

-include $(top_srcdir)/git.mk
