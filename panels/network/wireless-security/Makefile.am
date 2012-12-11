noinst_LTLIBRARIES = libwireless-security.la

libwireless_security_la_SOURCES = \
	wireless-security.h \
	wireless-security.c \
	ws-wep-key.h \
	ws-wep-key.c \
	ws-wpa-psk.h \
	ws-wpa-psk.c \
	ws-leap.h \
	ws-leap.c \
	ws-wpa-eap.h \
	ws-wpa-eap.c \
	ws-dynamic-wep.h \
	ws-dynamic-wep.c \
	eap-method.h \
	eap-method.c \
	eap-method-tls.h \
	eap-method-tls.c \
	eap-method-leap.h \
	eap-method-leap.c \
	eap-method-fast.h \
	eap-method-fast.c \
	eap-method-ttls.h \
	eap-method-ttls.c \
	eap-method-peap.h \
	eap-method-peap.c \
	eap-method-simple.h \
	eap-method-simple.c \
	helpers.h \
	helpers.c

libwireless_security_la_CPPFLAGS = \
	$(NETWORK_PANEL_CFLAGS) \
        $(NETWORK_MANAGER_CFLAGS)                       \
	-DUIDIR=\""$(uidir)"\"

libwireless_security_la_LIBADD = \
	$(NETWORK_PANEL_LIBS) \
	$(NETWORK_MANAGER_LIBS)

uidir = $(pkgdatadir)/ui
ui_DATA = \
	eap-method-leap.ui \
	eap-method-fast.ui \
	eap-method-peap.ui \
	eap-method-simple.ui \
	eap-method-tls.ui \
	eap-method-ttls.ui \
	nag-user-dialog.ui \
	ws-dynamic-wep.ui \
	ws-leap.ui \
	ws-wep-key.ui \
	ws-wpa-eap.ui \
	ws-wpa-psk.ui

EXTRA_DIST = \
	$(ui_DATA)

