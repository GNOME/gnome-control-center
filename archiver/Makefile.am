Locationmetadir = $(datadir)/hcm/default
Locationmeta_DATA = default-user.xml default-global.xml

includedir = $(prefix)/include/helix-archiver

INCLUDES = 			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I$(includedir) $(GNOME_INCLUDEDIR)		\
	-DVERSION=\""$(VERSION)"\"			\
	-DCONFIGDIR=\""/etc"\"				\
	-DLOCATION_DIR=\""$(datadir)/hcm/default"\"	\
	$(GNOME_XML_CFLAGS)

bin_PROGRAMS = helix-archiver
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
	$(GNOME_LIBDIR)					\
	$(GNOMEUI_LIBS)					\
	$(INTLLIBS)					\
	$(GNOME_XML_LIB)				\
	libhelix_archiver.a

