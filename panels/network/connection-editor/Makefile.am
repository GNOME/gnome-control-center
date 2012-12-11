noinst_LTLIBRARIES = libconnection-editor.la

libconnection_editor_la_SOURCES = 		\
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
        $(NETWORK_MANAGER_CFLAGS)		\
	-DGNOMECC_UI_DIR=\""$(uidir)"\"

libconnection_editor_la_LIBADD = 		\
	$(builddir)/../wireless-security/libwireless-security.la \
	$(NETWORK_PANEL_LIBS) 			\
	$(NETWORK_MANAGER_LIBS)

uidir = $(pkgdatadir)/ui
ui_DATA = 					\
	connection-editor.ui			\
	details-page.ui				\
	wifi-page.ui				\
	ip4-page.ui				\
	ip6-page.ui				\
	reset-page.ui				\
	security-page.ui

EXTRA_DIST = \
	$(ui_DATA)

