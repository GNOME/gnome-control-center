confexecdir = $(libdir)
confexec_DATA = config_archiverConf.sh

Locationmetadir = $(datadir)/control-center/archiver
Locationmeta_DATA = default-user.xml default-global.xml

includedir = $(prefix)/include/config-archiver

INCLUDES = 			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 				\
	-I$(includedir)								\
	$(GNOME_INCLUDEDIR)							\
	-DVERSION=\""$(VERSION)"\"						\
	-DG_LOG_DOMAIN=\"libconfig-archiver\"					\
	-DCONFIGDIR=\""/etc"\"							\
	-DLOCATION_DIR=\""$(datadir)/control-center/archiver"\"			\
	-DGLADE_DIR=\""$(INTERFACES_DIR)"\"							\
	-DXST_BACKEND_LOCATION=\""$(datadir)/setup-tool-backends/scripts"\"	\
	-DDEFAULTS_DIR=\""$(datadir)/control-center/defaults"\"			\
	@ARCHIVER_CFLAGS@							\
	@MONIKER_CFLAGS@

if MONIKER_ENABLE
BONOBO_MONIKER_ARCHIVER_BIN=bonobo-moniker-archiver
else
BONOBO_MONIKER_ARCHIVER_BIN=
endif

bin_PROGRAMS = $(BONOBO_MONIKER_ARCHIVER_BIN) config-archiver

lib_LTLIBRARIES = libconfig_archiver.la

libconfig_archiver_la_SOURCES = \
	archive.c		archive.h		\
	location.c		location.h		\
	config-log.c		config-log.h		\
	backend-list.c		backend-list.h		\
	cluster.c       	cluster.h		\
	cluster-location.c	cluster-location.h	\
	util.c			util.h

libconfig_archiver_la_LIBADD = \
	@GNOME_XML_LIBS@

include_HEADERS = \
	archive.h 		\
	location.h		\
	config-log.h		\
	backend-list.h		\
	cluster.h		\
	cluster-location.h	\
	util.h

config_archiver_SOURCES = \
	main.c

config_archiver_LDADD = \
	$(GNOME_LIBDIR)						\
	$(GNOMEUI_LIBS)						\
	$(INTLLIBS)						\
	@ARCHIVER_LIBS@						\
	libconfig_archiver.la

#
# Create the config_archiverConf.sh file from the sh.in file
#
config_archiverConf.sh: config_archiverConf.sh.in Makefile
	sed -e 's?\@VERSION\@?$(VERSION)?' \
	    -e 's?\@CONFIG_ARCHIVER_LIBDIR\@?$(CONFIG_ARCHIVER_LIBDIR)?g' \
	    -e 's?\@CONFIG_ARCHIVER_LIBS\@?$(CONFIG_ARCHIVER_LIBS)?g' \
	    -e 's?\@CONFIG_ARCHIVER_INCLUDEDIR\@?$(CONFIG_ARCHIVER_INCLUDEDIR)?g' \
	    < $(srcdir)/config_archiverConf.sh.in > config_archiverConf.tmp \
	  && mv config_archiverConf.tmp config_archiverConf.sh

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

bonobo_moniker_archiver_LDADD = @MONIKER_LIBS@ libconfig_archiver.la

EXTRA_DIST = \
	$(Locationmeta_DATA)			\
	config_archiverConf.sh.in		\
	Bonobo_Moniker_archiver.oaf.in
