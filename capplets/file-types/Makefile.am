INCLUDES = -I. 						\
	-I$(top_srcdir) 				\
	-I$(srcdir) 					\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	-I$(top_srcdir)/libgnomevfs			\
	$(CAPPLET_INCLUDEDIR)				\
	$(OAF_CFLAGS)					\
	$(GNOME_INCLUDEDIR)				\
	$(GTK_CFLAGS)					\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(includedir)					\
	$(VFS_CFLAGS) $(WERROR)

bin_PROGRAMS = file-types-capplet

file_types_capplet_SOURCES =			\
	nautilus-mime-type-capplet.h 		\
	nautilus-mime-type-capplet-dialogs.h 	\
	nautilus-mime-type-icon-entry.h		\
	nautilus-mime-type-capplet.c		\
	nautilus-mime-type-capplet-dialogs.c 	\
	nautilus-mime-type-icon-entry.c

file_types_capplet_LDADD =						\
			$(CAPPLET_LIBDIR) 				\
			$(CAPPLET_LIBS) 				\
			$(ORBIT_LIBS) 					\
			$(OAF_LIBS)					\
			$(INTLLIBS) 					\
			$(top_builddir)/libgnomevfs/libgnomevfs.la 	\
			-lgdk_pixbuf


sysdir = $(datadir)/control-center
sys_DATA = files-types-capplet.desktop 

settingsdir = $(datadir)/gnome/apps/Settings
settings_DATA = files-types-capplet.desktop

EXTRA_DIST = \
	$(sys_DATA)






