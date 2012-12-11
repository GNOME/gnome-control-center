noinst_LTLIBRARIES = libwireless-security.la

BUILT_SOURCES = \
	wireless-security-resources.h \
	wireless-security-resources.c

libwireless_security_la_SOURCES = \
	$(BUILT_SOURCES) \
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

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/wireless-security.gresource.xml)
wireless-security-resources.c: wireless-security.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name wireless_security $<
wireless-security-resources.h: wireless-security.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name wireless_security $<

EXTRA_DIST = \
	$(resource_files)

