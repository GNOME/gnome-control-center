INCLUDES =					\
	-I$(top_srcdir)/libslab			\
	$(GNOMECC_CFLAGS)

bin_PROGRAMS = gnome-control-center 

gnome_control_center_SOURCES =		\
	control-center.c

gnome_control_center_LDADD =						\
	$(GNOMECC_LIBS)							\
	$(top_builddir)/libslab/libslab.la

AM_CPPFLAGS =							\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"
