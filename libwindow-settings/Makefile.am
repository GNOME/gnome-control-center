INCLUDES = 								\
        -DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 		\
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\"			\
	-DG_LOG_DOMAIN=\"capplet-common\"				\
	-DGNOME_WINDOW_MANAGER_MODULE_PATH=\""$(libdir)/window-manager-settings"\" \
	-I$(top_srcdir)/						\
	@CAPPLET_CFLAGS@

lib_LTLIBRARIES = libgnome-window-settings.la

libgnome_window_settings_la_LDFLAGS = 	\
	-export_dynamic			\
        -version-info 1:0:0

libgnome_window_settings_la_SOURCES = \
	gnome-window-manager.c	\
	gnome-window-manager.h	\
	gnome-wm-manager.c	\
	gnome-wm-manager.h	\
	wm-exec.c		\
	wm-list.c		\
	wm-properties.h

libgnome_window_settingsincludedir = $(includedir)/gnome-window-settings-2.0

libgnome_window_settingsinclude_HEADERS =	\
	gnome-window-manager.h			\
	gnome-wm-manager.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gnome-window-settings-2.0.pc

EXTRA_DIST = 					\
	gnome-window-settings-2.0.pc.in		\
	ChangeLog