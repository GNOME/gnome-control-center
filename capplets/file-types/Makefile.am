NULL =

SUBDIRS = libuuid

INCLUDES = -I. 						\
	-I$(top_srcdir) 				\
	-I$(srcdir) 					\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	-I$(top_srcdir)/libgnomevfs			\
	-I$(srcdir)/../../control-center		\
	$(GNOME_INCLUDEDIR)				\
	$(GTK_CFLAGS)					\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(includedir)					\
	$(VFS_CFLAGS) $(WERROR)				\
	$(NULL)


bin_PROGRAMS = file-types-capplet

file_types_capplet_SOURCES =			\
	file-types-capplet.h 		\
	file-types-capplet-dialogs.h 	\
	file-types-icon-entry.h		\
	file-types-capplet.c		\
	file-types-capplet-dialogs.c 	\
	file-types-icon-entry.c		\
	$(NULL)


file_types_capplet_LDADD =			\
	../../control-center/libcapplet.la	\
	$(ORBIT_LIBS)				\
	$(INTLLIBS)				\
	$(VFS_LIBS)				\
	-lgdk_pixbuf				\
	$/libuuid/libuuid.a			\
	$(NULL)


sysdir = $(datadir)/control-center/Documents
sys_in_files = file-types-capplet.desktop.in 
sys_DATA = $(sys_in_files:.desktop.in=.desktop)

settingsdir = $(datadir)/gnome/apps/Settings
settings_DATA = $(sys_DATA)

desktop_in_file = file-types-capplet.desktop.in

desktop_file = $(desktop_in_file:.desktop.in=.desktop)

@XML_I18N_MERGE_DESKTOP_RULE@

EXTRA_DIST = \
	$(sys_DATA) $(desktop_in_file)






