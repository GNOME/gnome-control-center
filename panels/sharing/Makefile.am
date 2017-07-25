# This is used in PANEL_CFLAGS
cappletname = sharing

uidir = $(pkgdatadir)/ui/sharing
dbus_built_sources = org.gnome.SettingsDaemon.Sharing.c org.gnome.SettingsDaemon.Sharing.h

AM_CPPFLAGS =						\
	$(PANEL_CFLAGS)					\
	$(SHARING_PANEL_CFLAGS)				\
	-DLIBEXECDIR=\"$(libexecdir)\"			\
	-DSYSCONFDIR=\"$(sysconfdir)\"			\
	$(NULL)

noinst_LTLIBRARIES = libsharing.la

BUILT_SOURCES = 		\
	cc-sharing-resources.c	\
	cc-sharing-resources.h	\
	$(dbus_built_sources)	\
	$(NULL)

libsharing_la_SOURCES =		\
	$(BUILT_SOURCES)	\
	cc-sharing-panel.c	\
	cc-sharing-panel.h	\
	cc-remote-login.c	\
	cc-remote-login.h	\
	cc-media-sharing.c	\
	cc-media-sharing.h	\
	vino-preferences.c	\
	vino-preferences.h	\
	file-share-properties.c \
	file-share-properties.h \
	cc-sharing-networks.c	\
	cc-sharing-networks.h	\
	cc-sharing-switch.c	\
	cc-sharing-switch.h	\
	gsd-sharing-enums.h	\
	$(NULL)


resource_files = $(shell glib-compile-resources --generate-dependencies $(srcdir)/sharing.gresource.xml)
cc-sharing-resources.c: sharing.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_sharing $<
cc-sharing-resources.h: sharing.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_sharing $<

$(dbus_built_sources) : Makefile.am org.gnome.SettingsDaemon.Sharing.xml
	$(AM_V_GEN) gdbus-codegen \
	--interface-prefix org.gnome.SettingsDaemon. \
	--c-namespace Gsd \
	--generate-c-code org.gnome.SettingsDaemon.Sharing \
	$(srcdir)/org.gnome.SettingsDaemon.Sharing.xml

libsharing_la_LIBADD = $(PANEL_LIBS) $(SHARING_PANEL_LIBS)
libsharing_la_LDFLAGS = $(PANEL_LDFLAGS)

libexec_PROGRAMS = cc-remote-login-helper

cc_remote_login_helper_SOURCES = cc-remote-login-helper.c
cc_remote_login_helper_LDADD = $(REMOTE_LOGIN_HELPER_LIBS)
cc_remote_login_helper_CFLAGS = $(REMOTE_LOGIN_HELPER_CFLAGS)

org.gnome.controlcenter.remote-login-helper.policy.in: org.gnome.controlcenter.remote-login-helper.policy.in.in Makefile
	$(AM_V_GEN) sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

@INTLTOOL_POLICY_RULE@
polkit_policydir = $(datadir)/polkit-1/actions
polkit_policy_in_files = org.gnome.controlcenter.remote-login-helper.policy.in
polkit_policy_DATA = $(polkit_policy_in_files:.policy.in=.policy)


@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_in_files = gnome-sharing-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA) $(polkit_policy_DATA) $(polkit_policy_in_files) $(BUILT_SOURCES)
EXTRA_DIST = $(resource_files) sharing.gresource.xml org.gnome.controlcenter.remote-login-helper.policy.in.in org.gnome.SettingsDaemon.Sharing.xml meson.build

-include $(top_srcdir)/git.mk
