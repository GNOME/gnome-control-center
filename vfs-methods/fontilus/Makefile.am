
INCLUDES = $(FONT_METHOD_CFLAGS) $(THUMBNAILER_CFLAGS) $(FONT_VIEW_CFLAGS) \
  -DDIRECTORY_DIR=\"$(vfsdirectorydir)\" 

vfsmoduledir = $(libdir)/gnome-vfs-2.0/modules
vfsmodule_LTLIBRARIES = libfont-method.la

noinst_PROGRAMS = thumbnailer font-view

libfont_method_la_LDFLAGS = -module -avoid-version
libfont_method_la_LIBADD = $(FONT_METHOD_LIBS)
libfont_method_la_SOURCES = font-method.c

vfsmoduleconfdir = $(sysconfdir)/gnome-vfs-2.0/modules
vfsmoduleconf_DATA = font-method.conf

vfsdirectorydir = $(datadir)/gnome/vfolders
vfsdirectory_DATA = font-method.directory

thumbnailer_LDADD = $(THUMBNAILER_LIBS)
thumbnailer_SOURCES = ftstream-vfs.c thumbnailer.c

font_view_LDADD = $(FONT_VIEW_LIBS)
font_view_SOURCES = ftstream-vfs.c font-view.c

EXTRA_DIST = \
  font-method.conf \
  font-method.directory
