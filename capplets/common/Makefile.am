EXTRA_DIST = ChangeLog wrapper-script.in Bonobo_Control_Capplet_generic.oaf.in

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 		\
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\"			\
	-DG_LOG_DOMAIN=\"capplet-common\"				\
	-DINSTALL_PREFIX=\"$(prefix)\"					\
	-I$(top_srcdir)/libbackground					\
	-I$(top_srcdir)/libwindow-settings				\
	@VFS_CAPPLET_CFLAGS@						\
	@GNOME_DESKTOP_CFLAGS@						\
	@METACITY_CFLAGS@


noinst_LTLIBRARIES = libcommon.la

libcommon_la_SOURCES = \
	activate-settings-daemon.c	activate-settings-daemon.h	\
	capplet-util.c			capplet-util.h			\
	gconf-property-editor.c		gconf-property-editor.h		\
	gconf-property-editor-marshal.c	gconf-property-editor-marshal.h \
	file-transfer-dialog.c		file-transfer-dialog.h		\
	gnome-theme-info.c		gnome-theme-info.h		\
	wm-common.c			wm-common.h			\
	capplet-stock-icons.c		capplet-stock-icons.h		\
	theme-thumbnail.c		theme-thumbnail.h		\
	gnome-theme-apply.c		gnome-theme-apply.h

libcommon_la_LIBADD = $(top_builddir)/libbackground/libbackground.la	\
	$(top_builddir)/libwindow-settings/libgnome-window-settings.la  \
	@METACITY_LIBS@							\
	@GNOME_DESKTOP_LIBS@

gnome_theme_test_SOURCES = \
	gnome-theme-test.c

gnome_theme_test_LDADD = 						\
	$(GNOMECC_CAPPLETS_LIBS)

noinst_PROGRAMS = \
	gnome-theme-test
