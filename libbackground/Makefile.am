EXTRA_DIST = ChangeLog

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 		\
	-DG_LOG_DOMAIN=\"capplet-common\"				\
	-I$(top_srcdir)/						\
	@CAPPLET_CFLAGS@

noinst_LTLIBRARIES = libbackground.la

libbackground_la_SOURCES = \
	applier.c	applier.h	\
	preferences.c	preferences.h
