
noinst_LTLIBRARIES = libacme.la

if HAVE_ALSA
ALSA_CFLAGS = -DHAVE_ALSA
ALSA_files = acme-volume-alsa.c acme-volume-alsa.h
endif

if HAVE_OSS
OSS_CFLAGS = -DHAVE_OSS
OSS_files = acme-volume-oss.c acme-volume-oss.h
endif

if HAVE_FB
FB_CFLAGS = -DHAVE_FB
FB_files = acme-fb-level.c acme-fb-level.h
endif

INCLUDES = $(GNOME_CFLAGS) -I$(top_srcdir) $(ALSA_CFLAGS) $(OSS_CFLAGS) $(FB_CFLAGS)

libacme_la_SOURCES = \
	$(FB_files) $(OSS_files) $(ALSA_files)	\
	acme-volume.c acme-volume.h		\
	acme-volume-dummy.c acme-volume-dummy.h

Datadir = $(datadir)/control-center-2.0/interfaces/
Data_DATA = acme.glade

pixmapsdir = $(GNOMECC_PIXMAPS_DIR)
pixmaps_DATA = acme-brightness.png acme-eject.png gnome-speakernotes-muted.png gnome-speakernotes.png

EXTRA_DIST =					\
	acme-volume-alsa.c acme-volume-alsa.h	\
	acme-volume-oss.c acme-volume-oss.h	\
	acme-fb-level.c acme-fb-level.h		\
	acme.h					\
	acme.glade				\
	$(pixmaps_DATA)

