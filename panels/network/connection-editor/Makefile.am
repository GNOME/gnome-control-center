noinst_LTLIBRARIES = libconnection-editor.la

BUILT_SOURCES =					\
	net-connection-editor-resources.c	\
	net-connection-editor-resources.h

libconnection_editor_la_SOURCES = 		\
	$(BUILT_SOURCES)			\
	net-connection-editor.h			\
	net-connection-editor.c			\
	ce-page.h				\
	ce-page.c				\
	ce-page-details.h			\
	ce-page-details.c			\
	ce-page-wifi.h				\
	ce-page-wifi.c				\
	ce-page-ip4.h				\
	ce-page-ip4.c				\
	ce-page-ip6.h				\
	ce-page-ip6.c				\
	ce-page-security.h			\
	ce-page-security.c			\
	ce-page-ethernet.h			\
	ce-page-ethernet.c			\
	ce-page-8021x-security.h		\
	ce-page-8021x-security.c		\
	ce-page-vpn.h				\
	ce-page-vpn.c				\
	vpn-helpers.h				\
	vpn-helpers.c				\
	ui-helpers.h				\
	ui-helpers.c

libconnection_editor_la_CPPFLAGS = 		\
	$(PANEL_CFLAGS)				\
	-I$(srcdir)/../wireless-security	\
	$(NETWORK_PANEL_CFLAGS) 		\
        $(NETWORK_MANAGER_CFLAGS)		\
	-DNM_VPN_CONFIG_DIR=\""$(NM_VPN_CONFIG_DIR)"\" \
	-DNM_VPN_MODULE_DIR=\""$(NM_VPN_MODULE_DIR)"\"

libconnection_editor_la_LIBADD = 		\
	$(builddir)/../wireless-security/libwireless-security.la \
	$(NETWORK_PANEL_LIBS) 			\
	$(NETWORK_MANAGER_LIBS)

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/connection-editor.gresource.xml)
net-connection-editor-resources.c: connection-editor.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name net_connection_editor $<
net-connection-editor-resources.h: connection-editor.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name net_connection_editor $<

EXTRA_DIST = \
	$(resource_files) connection-editor.gresource.xml meson.build

-include $(top_srcdir)/git.mk
