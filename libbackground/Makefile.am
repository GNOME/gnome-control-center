EXTRA_DIST = ChangeLog

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" 			\
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\"			\
	-DG_LOG_DOMAIN=\"capplet-common\"				\
	-I$(top_srcdir)/						\
	-I$(top_srcdir)/intl						\
	@CAPPLET_CFLAGS@

noinst_LIBRARIES = libbackground.a

libbackground_a_SOURCES = \
	applier.c	applier.h	\
	preferences.c	preferences.h

