confexecdir = $(libdir)
confexec_DATA = ximian_archiverConf.sh

Locationmetadir = $(datadir)/ximian-setup-tools/archiver
Locationmeta_DATA = default-user.xml default-global.xml

includedir = $(prefix)/include/ximian-archiver

INCLUDES = 			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 				\
	-I$(includedir)								\
	$(GNOME_INCLUDEDIR)							\
	-DVERSION=\""$(VERSION)"\"						\
	-DG_LOG_DOMAIN=\"libximian-archiver\"					\
	-DCONFIGDIR=\""/etc"\"							\
	-DLOCATION_DIR=\""$(datadir)/ximian-setup-tools/archiver"\"		\
	-DGLADE_DIR=\""$(INTERFACES_DIR)"\"					\
	-DXST_BACKEND_LOCATION=\""$(datadir)/ximian-setup-tools/scripts"\"	\
	@ARCHIVER_CFLAGS@							\
	@MONIKER_CFLAGS@

bin_PROGRAMS = bonobo-moniker-archiver ximian-archiver
lib_LTLIBRARIES = libximian_archiver.la

libximian_archiver_la_SOURCES = \
	archive.c		archive.h		\
	location.c		location.h		\
	config-log.c		config-log.h		\
	backend-list.c		backend-list.h		\
	cluster.c       	cluster.h		\
	cluster-location.c	cluster-location.h	\
	util.c			util.h

libximian_archiver_la_LIBADD = \
	@GNOME_XML_LIBS@

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
	$(GNOME_LIBDIR)						\
	$(GNOMEUI_LIBS)						\
	$(INTLLIBS)						\
	@ARCHIVER_LIBS@						\
	libximian_archiver.la

#
# Create the ximian_archiverConf.sh file from the sh.in file
#
ximian_archiverConf.sh: ximian_archiverConf.sh.in Makefile
	sed -e 's?\@VERSION\@?$(VERSION)?' \
	    -e 's?\@XIMIAN_ARCHIVER_LIBDIR\@?$(XIMIAN_ARCHIVER_LIBDIR_CONF)?g' \
	    -e 's?\@XIMIAN_ARCHIVER_LIBS\@?$(XIMIAN_ARCHIVER_LIBS_CONF)?g' \
	    -e 's?\@XIMIAN_ARCHIVER_INCLUDEDIR\@?$(XIMIAN_ARCHIVER_INCLUDEDIR_CONF)?g' \
	    < $(srcdir)/ximian_archiverConf.sh.in > ximian_archiverConf.tmp \
	  && mv ximian_archiverConf.tmp ximian_archiverConf.sh

OAF_FILES = 						\
	Bonobo_Moniker_archiver.oaf			

oafdir = $(datadir)/oaf
oaf_DATA = $(OAF_FILES)

monikerdir = $(libdir)/bonobo/monikers

Bonobo_Moniker_archiver.oaf : $(srcdir)/Bonobo_Moniker_archiver.oaf.in $(top_builddir)/config.status
	sed -e "s|\@MONIKER_LIBDIR\@|$(monikerdir)|" \
	$(srcdir)/Bonobo_Moniker_archiver.oaf.in > Bonobo_Moniker_archiver.oaf 

clean-local:
	-rm -f $(OAF_FILES)

bonobo_moniker_archiver_SOURCES =		\
	bonobo-config-archiver.c		\
	bonobo-config-archiver.h		\
	bonobo-moniker-archiver.c

bonobo_moniker_archiver_LDADD = @MONIKER_LIBS@ libximian_archiver.la

EXTRA_DIST = \
	$(Locationmeta_DATA)			\
	ximian_archiverConf.sh.in		\
	Bonobo_Moniker_archiver.oaf.in
