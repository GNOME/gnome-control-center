
INCLUDES = $(FONT_METHOD_CFLAGS) $(THUMBNAILER_CFLAGS)

vfsmoduledir = $(libdir)/gnome-vfs-2.0/modules
vfsmodule_LTLIBRARIES = libfont-method.la

noinst_PROGRAMS = thumbnailer

libfont_method_la_LDFLAGS = -module -avoid-version
libfont_method_la_LIBS = $(FONT_METHOD_LIBS)
libfont_method_la_SOURCES = font-method.c

vfsmoduleconfdir = $(sysconfdir)/gnome-vfs-2.0/modules
vfsmoduleconf_DATA = font-method.conf

thumbnailer_LDADD = $(THUMBNAILER_LIBS)
thumbnailer_SOURCES = ftstream-vfs.c thumbnailer.c

EXTRA_DIST = \
  font-method.conf \
  font-method.directory
