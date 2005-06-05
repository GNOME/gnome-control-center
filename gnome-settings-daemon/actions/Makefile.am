INCLUDES = $(GNOME_CFLAGS) -I$(top_srcdir) $(GST_CFLAGS)

noinst_LTLIBRARIES = libacme.la

libacme_la_SOURCES = \
	acme-volume.c acme-volume.h

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
else # HAVE_OSS
libacme_la_SOURCES += acme-volume-dummy.c acme-volume-dummy.h
endif # HAVE_OSS
endif # HAVE_ALSA
endif # HAVE_GSTREAMER

if HAVE_FB
INCLUDES += -DHAVE_FB
libacme_la_SOURCES += acme-fb-level.c acme-fb-level.h
endif

if HAVE_GSTREAMER
INCLUDES += -DHAVE_GSTREAMER
libacme_la_SOURCES += acme-volume-gstreamer.c acme-volume-gstreamer.h
libacme_la_LIBADD += $(GST_LIBS)
endif

Datadir = $(datadir)/control-center-2.0/interfaces/
Data_DATA = acme.glade

pixmapsdir = $(GNOMECC_PIXMAPS_DIR)
pixmaps_DATA = acme-brightness.png acme-eject.png gnome-speakernotes-muted.png gnome-speakernotes.png

EXTRA_DIST =					\
	acme-volume-alsa.c acme-volume-alsa.h	\
	acme-volume-gstreamer.c acme-volume-gstreamer.h	\
	acme-volume-oss.c acme-volume-oss.h	\
	acme-volume-dummy.c acme-volume-dummy.h	\
	acme-fb-level.c acme-fb-level.h		\
	acme.h					\
	acme.glade				\
	$(pixmaps_DATA)
