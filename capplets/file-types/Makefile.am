INCLUDES = -I. -I$(srcdir) 				\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	$(CAPPLET_INCLUDEDIR) \
	$(GNOME_INCLUDEDIR) \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(includedir)

bin_PROGRAMS = nautilus-mime-type-capplet

nautilus_mime_type_capplet_SOURCES =\
	nautilus-mime-type-capplet.c\
	nautilus-mime-type-capplet-dialogs.c\
	mime-data.h \
	mime-data.c \
	edit-window.c \
	edit-window.h \
	mime-info.c \
	mime-info.h \
	new-mime-window.h \
	new-mime-window.c

nautilus_mime_type_capplet_LDADD =\
			$(CAPPLET_LIBDIR) \
			$(CAPPLET_LIBS) \
			$(ORBIT_LIBS) \
			$(INTLLIBS) \
			$(top_builddir)/libgnomevfs/libgnomevfs.la


sysdir = $(datadir)/control-center
sys_DATA = nautilus-mime-type.desktop 

install-data-local:
	$(INSTALL_DATA) -D -c $(srcdir)/nautilus-mime-type.desktop $(DESTDIR)$(datadir)/gnome/apps/Settings/nautilus-mime-type.desktop

EXTRA_DIST = \
	$(sys_DATA)






