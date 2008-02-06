EXTRA_DIST = ChangeLog

INCLUDES = \
	-DG_LOG_DOMAIN=\"capplet-common\"				\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""			\
	-I$(top_srcdir)/						\
	@LIBBACKGROUND_CFLAGS@

noinst_LTLIBRARIES = libbackground.la

libbackground_la_SOURCES = \
	preferences.c	preferences.h
