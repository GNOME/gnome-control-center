Locationmetadir = $(datadir)/hcm/default
Locationmeta_DATA = default-user.xml default-global.xml

INCLUDES = 			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I$(includedir) $(GNOME_INCLUDEDIR)		\
	-DVERSION=\""$(VERSION)"\"			\
	-DCONFIGDIR=\""/etc"\"				\
	-DLOCATION_DIR=\""$(datadir)/hcm/default"\"	\
	$(XML_CFLAGS)

bin_PROGRAMS = archiver

archiver_SOURCES = \
	main.c						\
	archive.c	archive.h			\
	location.c	location.h			\
	config-log.c	config-log.h			\
	backend-list.c	backend-list.h			\
	util.c		util.h

archiver_LDADD = \
	$(GNOME_LIBDIR)					\
	$(GNOMEUI_LIBS)					\
	$(INTLLIBS)					\
	$(GNOME_XML_LIB)
