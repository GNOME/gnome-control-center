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
	ce-page-reset.h				\
	ce-page-reset.c

libconnection_editor_la_CPPFLAGS = 		\
	-I$(srcdir)/../wireless-security	\
	$(NETWORK_PANEL_CFLAGS) 		\
        $(NETWORK_MANAGER_CFLAGS)

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
	$(resource_files)
