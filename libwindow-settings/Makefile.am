EXTRA_DIST = ChangeLog

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" 			\
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\"			\
	-DG_LOG_DOMAIN=\"capplet-common\"				\
	-DGNOME_WINDOW_MANAGER_MODULE_PATH=\""$(libdir)/window-manager-settings"\" \
	-I$(top_srcdir)/						\
	@CAPPLET_CFLAGS@

noinst_LTLIBRARIES = libwindow-settings.la

libwindow_settings_la_SOURCES = \
	gnome-window-manager.c	\
	gnome-window-manager.h

libwindow_settings_la_LIBADD = $(GNOMECC_CAPPLETS_LIBS)