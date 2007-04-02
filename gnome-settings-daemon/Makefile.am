SUBDIRS = xrdb actions

INCLUDES= \
	$(DBUS_CFLAGS)					\
	$(LIBGNOMEKBD_CFLAGS)				\
	$(GNOME_SETTINGS_DAEMON_CFLAGS)			\
	$(FB_CFLAGS)					\
	-I$(top_srcdir)/libbackground			\
	-I$(top_srcdir)					\
	-DDATADIR="\"$(datadir)\""			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-DGNOMECC_GLADE_DIR="\"$(pkgdatadir)/glade\""	\
	-DGNOMECC_PIXMAPS_DIR="\"$(pkgdatadir)/pixmaps\""

libexec_PROGRAMS=gnome-settings-daemon

gnome_settings_daemon_SOURCES =	\
	clipboard-manager.c		\
	clipboard-manager.h		\
	delayed-dialog.c		\
	eggaccelerators.h		\
	eggaccelerators.c		\
	factory.c			\
	gnome-settings-accessibility-keyboard.h	\
	gnome-settings-accessibility-keyboard.c	\
	gnome-settings-background.h	\
	gnome-settings-background.c	\
	gnome-settings-daemon.h		\
	gnome-settings-daemon.c		\
	gnome-settings-dbus.c		\
	gnome-settings-dbus.h		\
	gnome-settings-default-editor.c	\
	gnome-settings-default-editor.h	\
	gnome-settings-font.h		\
	gnome-settings-font.c		\
	gnome-settings-gtk1theme.c      \
	gnome-settings-gtk1theme.h	\
	gnome-settings-keybindings.c    \
	gnome-settings-keybindings.h    \
	gnome-settings-keyboard-xkb.h	\
	gnome-settings-keyboard-xkb.c	\
	gnome-settings-keyboard.h	\
	gnome-settings-keyboard.c	\
	gnome-settings-locate-pointer.c	\
	gnome-settings-locate-pointer.h	\
	gnome-settings-mouse.h		\
	gnome-settings-mouse.c		\
	gnome-settings-multimedia-keys.h \
	gnome-settings-multimedia-keys.c \
	gnome-settings-screensaver.h	\
	gnome-settings-screensaver.c	\
	gnome-settings-server.h		\
	gnome-settings-sound.c		\
	gnome-settings-sound.h		\
	gnome-settings-typing-break.c	\
	gnome-settings-typing-break.h	\
	gnome-settings-xmodmap.c	\
	gnome-settings-xmodmap.h	\
	gnome-settings-xrdb.c		\
	gnome-settings-xrdb.h		\
	gnome-settings-xsettings.c	\
	gnome-settings-xsettings.h	\
	gsd-media-keys-window.c		\
	gsd-media-keys-window.h		\
	list.c				\
	list.h				\
	reaper.c			\
	reaper.h			\
	utils.c				\
	utils.h				\
	xsettings-common.c		\
	xsettings-common.h		\
	xsettings-manager.c		\
	xsettings-manager.h		\
	xutils.c			\
	xutils.h

#	$(AccessX_files)		

gnome_settings_daemon_LDADD = \
	$(DBUS_LIBS)						\
	$(LIBGNOMEKBD_LIBS)						\
	$(top_builddir)/libbackground/libbackground.la			\
	$(top_builddir)/libsounds/libsounds.a				\
	$(top_builddir)/libwindow-settings/libgnome-window-settings.la 	\
	$(top_builddir)/gnome-settings-daemon/actions/libacme.la	\
	$(GNOME_SETTINGS_DAEMON_LIBS)					\
	$(XF86MISC_LIBS) $(GST_LIBS)

#AccessX_files = \
#	AccessXcomm.c		\
#	extutil.h		\
#	AccessXproto.h		\
#	AccessXlibint.h

BUILT_SOURCES=gnome-settings-server.h		\
	gnome-settings-client.h			\
	gnome-settings-marshal.h		\
	gnome-settings-marshal.c

gnome_settings_daemon_SOURCES+=$(BUILT_SOURCES)

Gladedir   = $(pkgdatadir)/glade
Glade_DATA = modmap-dialog.glade

Dbusapidir   = $(includedir)/gnome-settings-daemon-2.0/gnome-settings-daemon
Dbusapi_DATA = gnome-settings-client.h

#Rule to generate the marshal files
gnome-settings-marshal.c: gnome-settings-marshal.list
	@GLIB_GENMARSHAL@ --prefix=gnome_settings_marshal $< --header --body > $@

gnome-settings-marshal.h: gnome-settings-marshal.list
	@GLIB_GENMARSHAL@ --prefix=gnome_settings_marshal $< --header > $@

#Rule to generate the binding headers
gnome-settings-server.h:  gsd-infos.xml
	dbus-binding-tool --prefix=settings_daemon --mode=glib-server $< > $@

gnome-settings-client.h:  gsd-infos.xml
	dbus-binding-tool --prefix=settings_daemon --mode=glib-client $< > $@

# DBUS stuff
servicedir = $(datadir)/dbus-1/services
service_in_files = org.gnome.SettingsDaemon.service.in
service_DATA = $(service_in_files:.service.in=.service)

org.gnome.SettingsDaemon.service: org.gnome.SettingsDaemon.service.in Makefile
	@sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = $(Glade_DATA) $(Dbusapi_DATA) \
	gnome-settings-daemon.pc.in gsd-infos.xml \
	org.gnome.SettingsDaemon.service.in gnome-settings-marshal.list
CLEANFILES = $(BUILT_SOURCES) $(service_DATA)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gnome-settings-daemon.pc

noinst_PROGRAMS =				\
	test-media-window

test_media_window_SOURCES =			\
	gsd-media-keys-window.c			\
	gsd-media-keys-window.h			\
	test-media-window.c

test_media_window_LDADD = \
	$(GNOME_SETTINGS_DAEMON_LIBS)					\
	$(XF86MISC_LIBS) $(GST_LIBS)
