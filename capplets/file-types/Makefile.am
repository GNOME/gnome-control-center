SUBDIRS = libuuid

bin_PROGRAMS = gnome-file-types-properties

gnome_file_types_properties_LDADD = $(GNOMECC_CAPPLETS_LIBS) $(VFS_CAPPLET_LIBS) libuuid/libuuid.a

gnome_file_types_properties_SOURCES =					\
	file-types-capplet.c						\
	mime-types-model.c		mime-types-model.h		\
	model-entry.c			model-entry.h			\
	mime-type-info.c		mime-type-info.h		\
	mime-edit-dialog.c		mime-edit-dialog.h		\
	mime-category-edit-dialog.c	mime-category-edit-dialog.h	\
	service-info.c			service-info.h			\
	service-edit-dialog.c		service-edit-dialog.h

@INTLTOOL_DESKTOP_RULE@

Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = file-types-properties.glade

desktopdir = $(GNOMECC_DESKTOP_DIR)
Desktop_in_files = file-types.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS) $(VFS_CAPPLET_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Glade_DATA) $(Desktop_in_files)
