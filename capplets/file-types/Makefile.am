NULL =

SUBDIRS = libuuid

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
	$(VFS_CFLAGS) $(WERROR)				\
	$(NULL)


bin_PROGRAMS = file-types-capplet

file_types_capplet_SOURCES =			\
	nautilus-mime-type-capplet.h 		\
	nautilus-mime-type-capplet-dialogs.h 	\
	nautilus-mime-type-icon-entry.h		\
	nautilus-mime-type-capplet.c		\
	nautilus-mime-type-capplet-dialogs.c 	\
	nautilus-mime-type-icon-entry.c		\
	$(NULL)


file_types_capplet_LDADD =						\
			$(CAPPLET_LIBDIR) 				\
			$(CAPPLET_LIBS) 				\
			$(ORBIT_LIBS) 					\
			$(OAF_LIBS)					\
			$(INTLLIBS) 					\
			$(top_builddir)/libgnomevfs/libgnomevfs.la 	\
			-lgdk_pixbuf					\
			$/libuuid/libuuid.a				\
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






