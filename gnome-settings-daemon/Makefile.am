INCLUDES=$(GTK_CFLAGS) $(GCONF_CFLAGS)

gnome_settings_daemon_SOURCES =		\
	gnome-settings-daemon.h		\
	gnome-settings-daemon.c		\
	gnome-settings-mouse.h		\
	gnome-settings-mouse.c		\
	gnome-settings-xsettings.c	\
	gnome-settings-xsettings.h	\
	xsettings-common.c		\
	xsettings-manager.c		\
	xsettings-common.h		\
	xsettings-manager.h

bin_PROGRAMS=gnome-settings-daemon

gnome_settings_daemon_LDADD = $(GTK_LIBS) $(GCONF_LIBS)
