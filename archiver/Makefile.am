Locationmetadir = $(datadir)/hcm/default
Locationmeta_DATA = default-user.xml default-global.xml

gladedir = $(prefix)/share/hcm/glade
glade_DATA = rollback-location-management.glade

includedir = $(prefix)/include/helix-archiver

INCLUDES = 			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I$(includedir) $(GNOME_INCLUDEDIR)		\
	-DVERSION=\""$(VERSION)"\"			\
	-DCONFIGDIR=\""/etc"\"				\
	-DLOCATION_DIR=\""$(datadir)/hcm/default"\"	\
	-DGLADE_DIR=\""$(datadir)/hcm/glade"\"		\
	$(GNOME_XML_CFLAGS)				\
	$(LIBGLADE_CFLAGS)

bin_PROGRAMS = helix-archiver helix-config-manager
lib_LIBRARIES = libhelix_archiver.a

libhelix_archiver_a_SOURCES = \
	archive.c	archive.h			\
	location.c	location.h			\
	config-log.c	config-log.h			\
	backend-list.c	backend-list.h			\
	util.c		util.h

include_HEADERS = \
	archive.h location.h config-log.h backend-list.h util.h

helix_archiver_SOURCES = \
	main.c

helix_archiver_LDADD = \
	$(GNOME_LIBDIR)							\
	$(GNOMEUI_LIBS)							\
	$(INTLLIBS)							\
	$(GNOME_XML_LIB)						\
	libhelix_archiver.a

helix_config_manager_SOURCES = \
	config-manager.c						\
	config-manager-dialog.c		config-manager-dialog.h		\
	create-location-dialog.c	create-location-dialog.h	\
	rollback-widget.c		rollback-widget.h		\
	location-list.c			location-list.h

helix_config_manager_LDADD = \
	$(GNOME_LIBDIR)							\
	$(GNOMEUI_LIBS)							\
	$(INTLLIBS)							\
	$(GNOME_XML_LIB)						\
	$(LIBGLADE_LIBS)						\
	libhelix_archiver.a
