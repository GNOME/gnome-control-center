SUBDIRS = xrdb actions

INCLUDES=$(DBUS_CFLAGS) $(LIBXKLAVIER_CFLAGS) $(GNOME_SETTINGS_DAEMON_CFLAGS) -I$(top_srcdir)/libbackground -I$(top_srcdir) \
  -DGNOMELOCALEDIR="\"$(datadir)/locale\"" 	\
  -DESD_SERVER="\"$(ESD_SERVER)\""		\
  -DDATADIR="\"$(datadir)\""			\
  -DPIXMAPSDIR="\"$(GNOMECC_PIXMAPS_DIR)\""	\
  $(FB_CFLAGS)

libexec_PROGRAMS=gnome-settings-daemon

gnome_settings_daemon_SOURCES =	\
	factory.c			\
	eggaccelerators.h		\
	eggaccelerators.c		\
	gnome-settings-daemon.h		\
	gnome-settings-daemon.c		\
	gnome-settings-font.h		\
	gnome-settings-font.c		\
	gnome-settings-mouse.h		\
	gnome-settings-mouse.c		\
	gnome-settings-multimedia-keys.h \
	gnome-settings-multimedia-keys.c \
	gnome-settings-keyboard-xkb.h	\
	gnome-settings-keyboard-xkb.c	\
	gnome-settings-keyboard.h	\
	gnome-settings-keyboard.c	\
	gnome-settings-background.h	\
	gnome-settings-background.c	\
	gnome-settings-xsettings.c	\
	gnome-settings-xsettings.h	\
	gnome-settings-locate-pointer.c	\
	gnome-settings-locate-pointer.h	\
	gnome-settings-sound.c		\
	gnome-settings-sound.h		\
	gnome-settings-accessibility-keyboard.h	\
	gnome-settings-accessibility-keyboard.c	\
	gnome-settings-screensaver.h	\
	gnome-settings-screensaver.c	\
	gnome-settings-default-editor.c	\
	gnome-settings-default-editor.h	\
	gnome-settings-gtk1theme.c      \
	gnome-settings-gtk1theme.h	\
	gnome-settings-server.h		\
	gnome-settings-dbus.c		\
	gnome-settings-xrdb.c		\
	gnome-settings-xrdb.h		\
	xsettings-common.c		\
	xsettings-manager.c		\
	xsettings-common.h		\
	xsettings-manager.h		\
	gnome-settings-keybindings.c    \
	gnome-settings-keybindings.h    \
	gnome-settings-typing-break.c	\
	gnome-settings-typing-break.h	\
	reaper.c			\
	reaper.h			\
	gnome-settings-xmodmap.c	\
	gnome-settings-xmodmap.h	\
	list.c				\
	list.h				\
	xutils.c			\
	xutils.h			\
	clipboard-manager.c		\
	clipboard-manager.h		

#	$(AccessX_files)		

gnome_settings_daemon_LDADD = \
	$(DBUS_LIBS)						\
	$(LIBXKLAVIER_LIBS)						\
	$(top_builddir)/libbackground/libbackground.la			\
	$(top_builddir)/libsounds/libsounds.a				\
	$(top_builddir)/libgswitchit/libgswitchit.a			\
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
	gnome-settings-client.h

Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = modmap-dialog.glade

Dbusapidir   = $(includedir)/gnome-settings-daemon-2.0/gnome-settings-daemon
Dbusapi_DATA = gnome-settings-client.h

#Rule to generate the binding headers
gnome-settings-server.h:  gsd-infos.xml
	dbus-binding-tool --prefix=settings_daemon --mode=glib-server $< > $@

gnome-settings-client.h:  gsd-infos.xml
	dbus-binding-tool --prefix=settings_daemon --mode=glib-client $< > $@

# DBUS stuff
servicedir = $(DBUS_SERVICES_DIR)
service_in_files = org.gnome.SettingsDaemon.service.in
service_DATA = $(service_in_files:.service.in=.service)

org.gnome.SettingsDaemon.service: org.gnome.SettingsDaemon.service.in Makefile
	@sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

EXTRA_DIST = $(Glade_DATA) $(Dbusapi_DATA) \
             gnome-settings-daemon.pc.in gsd-infos.xml org.gnome.SettingsDaemon.service.in
CLEANFILES = $(BUILT_SOURCES)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gnome-settings-daemon.pc
