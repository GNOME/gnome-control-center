INCLUDES=$(GNOME_SETTINGS_DAEMON_CFLAGS) -I$(top_srcdir)/libbackground

gnome_settings_daemon_SOURCES =		\
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
	xsettings-common.c		\
	xsettings-manager.c		\
	xsettings-common.h		\
	xsettings-manager.h

bin_PROGRAMS=gnome-settings-daemon

gnome_settings_daemon_LDADD = \
	$(GNOME_SETTINGS_DAEMON_LIBS)			\
	$(top_builddir)/libbackground/libbackground.a	\
	$(XF86MISC_LIBS)

