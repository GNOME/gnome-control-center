AM_CPPFLAGS =					\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""\
	-I$(top_srcdir)				\
	$(SHELL_CFLAGS)				\
	$(CHEESE_CFLAGS)			\
	$(WACOM_PANEL_CFLAGS)			\
	-I$(top_srcdir)/shell			\
	-I$(top_srcdir)/panels/common		\
	-I$(top_srcdir)/libgd

noinst_LTLIBRARIES = libshell_alt.la

libshell_alt_la_SOURCES = 			\
	cc-window.c				\
	cc-window.h

libshell_alt_la_LIBADD = $(top_builddir)/libgd/libgd.la

-include $(top_srcdir)/git.mk
