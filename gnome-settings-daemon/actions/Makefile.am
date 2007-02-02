INCLUDES = $(GNOME_CFLAGS) -I$(top_srcdir) $(GST_CFLAGS)

noinst_LTLIBRARIES = libacme.la

libacme_la_SOURCES = \
	acme-volume.c acme-volume.h \
	acme-volume-dummy.c acme-volume-dummy.h

libacme_la_LIBADD = 

if HAVE_GSTREAMER
INCLUDES += -DHAVE_GSTREAMER
libacme_la_SOURCES += acme-volume-gstreamer.c acme-volume-gstreamer.h
libacme_la_LIBADD += $(GST_LIBS)
else # HAVE_GSTREAMER
if HAVE_ALSA
INCLUDES += -DHAVE_ALSA
libacme_la_SOURCES += acme-volume-alsa.c acme-volume-alsa.h
libacme_la_LIBADD += $(ALSA_LIBS)
else # HAVE_ALSA
if HAVE_OSS
INCLUDES += -DHAVE_OSS
libacme_la_SOURCES += acme-volume-oss.c acme-volume-oss.h
endif # HAVE_OSS
endif # HAVE_ALSA
endif # HAVE_GSTREAMER

Datadir = $(pkgdatadir)/glade
Data_DATA = acme.glade

pixmapsdir = $(pkgdatadir)/pixmaps
pixmaps_DATA = acme-eject.png

EXTRA_DIST =					\
	acme-volume-alsa.c acme-volume-alsa.h	\
	acme-volume-gstreamer.c acme-volume-gstreamer.h	\
	acme-volume-oss.c acme-volume-oss.h	\
	acme.h					\
	acme.glade				\
	$(pixmaps_DATA)
