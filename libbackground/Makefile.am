EXTRA_DIST = ChangeLog

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" 			\
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\"			\
	-DG_LOG_DOMAIN=\"capplet-common\"				\
	-I$(top_srcdir)/						\
	@CAPPLET_CFLAGS@

noinst_LTLIBRARIES = libbackground.la

libbackground_la_SOURCES = \
	applier.c	applier.h	\
	preferences.c	preferences.h   \
	preview-file-selection.c	preview-file-selection.h
	
