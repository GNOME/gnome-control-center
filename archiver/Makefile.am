SUBDIRS = location-manager-capplet

confexecdir = $(libdir)
confexec_DATA = ximian_archiverConf.sh

Locationmetadir = $(datadir)/ximian-config/default
Locationmeta_DATA = default-user.xml default-global.xml

gladedir = $(prefix)/share/hcm/glade
glade_DATA = rollback-location-management.glade

includedir = $(prefix)/include/ximian-archiver

INCLUDES = 			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-I$(includedir) $(GNOME_INCLUDEDIR)		\
	-DVERSION=\""$(VERSION)"\"			\
	-DG_LOG_DOMAIN=\"libximian-archiver\"		\
	-DCONFIGDIR=\""/etc"\"				\
	-DLOCATION_DIR=\""$(datadir)/ximian-config/default"\"	\
	-DGLADE_DIR=\""$(datadir)/ximian-config/glade"\"		\
	-DXST_BACKEND_LOCATION=\""$(datadir)/ximian-setup-tools/scripts"\"	\
	$(GNOME_XML_CFLAGS)				\
	$(LIBGLADE_CFLAGS)

bin_PROGRAMS = ximian-archiver
lib_LTLIBRARIES = libximian_archiver.la

libximian_archiver_la_SOURCES = \
	archive.c		archive.h		\
	location.c		location.h		\
	config-log.c		config-log.h		\
	backend-list.c		backend-list.h		\
	cluster.c       	cluster.h		\
	cluster-location.c	cluster-location.h	\
	util.c			util.h

include_HEADERS = \
	archive.h 		\
	location.h		\
	config-log.h		\
	backend-list.h		\
	cluster.h		\
	cluster-location.h	\
	util.h

ximian_archiver_SOURCES = \
	main.c

ximian_archiver_LDADD = \
	$(GNOME_LIBDIR)							\
	$(GNOMEUI_LIBS)							\
	$(INTLLIBS)							\
	$(GNOME_XML_LIB)						\
	libximian_archiver.la

EXTRA_DIST = \
	$(Locationmeta_DATA) \
	$(glade_DATA) \
	ximian_archiverConf.sh.in

#
# Create the ximian_archiverConf.sh file from the sh.in file
#
ximian_archiverConf.sh: ximian_archiverConf.sh.in Makefile
	sed -e 's?\@VERSION\@?$(VERSION)?' \
	    -e 's?\@GNOME_LIBDIR\@?$(GNOME_LIBDIR) -L$(prefix)/lib?' \
	    -e 's?\@GNOMEUI_LIBS\@?$(GNOMEUI_LIBS)?g' \
	    -e 's?\@GNOME_INCLUDEDIR\@?$(GNOME_INCLUDEDIR) -I$(prefix)/include?g' \
	    -e 's?\@ORB_LIBS\@?$(ORB_LIBS)?g' \
	    < $(srcdir)/ximian_archiverConf.sh.in > ximian_archiverConf.tmp \
	  && mv ximian_archiverConf.tmp ximian_archiverConf.sh


