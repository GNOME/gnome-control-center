
INCLUDES = $(FONT_METHOD_CFLAGS) $(THUMBNAILER_CFLAGS)

moduledir = $(libdir)/gnome-vfs-2.0/modules
module_LTLIBRARIES = libfont-method.la

noinst_PROGRAMS = thumbnailer

libfont_method_la_LDFLAGS = -module -avoid-version
libfont_method_la_LIBS = $(FONT_METHOD_LIBS)
libfont_method_la_SOURCES = font-method.c

thumbnailer_LDADD = $(THUMBNAILER_LIBS)
thumbnailer_SOURCES = ftstream-vfs.c thumbnailer.c
