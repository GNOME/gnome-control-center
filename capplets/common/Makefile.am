EXTRA_DIST = ChangeLog

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" 			\
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\"			\
	-DG_LOG_DOMAIN=\"capplet-common\"				\
	-DGLADE_DATADIR=\""$(datadir)/control-center/interfaces"\"	\
	-I$(top_srcdir)/						\
	-I$(top_srcdir)/intl						\
	@CAPPLET_CFLAGS@

noinst_LIBRARIES = libcommon.a

libcommon_a_SOURCES = \
	capplet-util.c		capplet-util.h
