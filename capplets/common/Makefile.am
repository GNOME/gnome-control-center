EXTRA_DIST = ChangeLog wrapper-script.in Bonobo_Control_Capplet_generic.oaf.in

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" 			\
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\"			\
	-DG_LOG_DOMAIN=\"capplet-common\"				\
	-I$(top_srcdir)/						\
	@CAPPLET_CFLAGS@

noinst_LIBRARIES = libcommon.a

libcommon_a_SOURCES = \
	activate-settings-daemon.c	activate-settings-daemon.h	\
	capplet-util.c			capplet-util.h			\
	gconf-property-editor.c		gconf-property-editor.h		\
	gconf-property-editor-marshal.c	gconf-property-editor-marshal.h \
	theme-common.c			theme-common.h
