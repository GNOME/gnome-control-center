INCLUDES=$(GNOME_SETTINGS_DAEMON_CFLAGS) -I$(top_srcdir)/libbackground -I$(top_srcdir)

bin_PROGRAMS=gnome2-settings-daemon

gnome2_settings_daemon_SOURCES =	\
	gnome-settings-daemon.h		\
	gnome-settings-daemon.c		\
	gnome-settings-mouse.h		\
	gnome-settings-mouse.c		\
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
	xsettings-common.c		\
	xsettings-manager.c		\
	xsettings-common.h		\
	xsettings-manager.h

gnome2_settings_daemon_LDADD = \
	$(GNOME_SETTINGS_DAEMON_LIBS)			\
	$(top_builddir)/libbackground/libbackground.a	\
	$(top_builddir)/libsounds/libsounds.a		\
	$(XF86MISC_LIBS)

