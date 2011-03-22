SUBDIRS = data

# This is used in PANEL_CFLAGS
cappletname = sound
NULL =

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libsound.la

bin_PROGRAMS =					\
	gnome-sound-applet			\
	$(NULL)

AM_CPPFLAGS =					\
	$(PANEL_CFLAGS) 			\
	$(SOUND_PANEL_CFLAGS)			\
	-DLOCALE_DIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\"$(libexecdir)\"		\
	-DGLADEDIR=\""$(pkgdatadir)"\"		\
        -DSOUND_DATA_DIR="\"$(datadir)/sounds\""	\
        -DSOUND_SET_DIR="\"$(pkgdatadir)/sounds\""	\
	-DICON_DATA_DIR="\"$(pkgdatadir)/icons\"" \
	$(NULL)

noinst_LTLIBRARIES = libgnomevolumecontrol.la
libgnomevolumecontrol_la_SOURCES =		\
	gvc-mixer-card.h			\
	gvc-mixer-card.c			\
	gvc-mixer-card-private.h		\
	gvc-mixer-stream.h			\
	gvc-mixer-stream.c			\
	gvc-mixer-stream-private.h		\
	gvc-channel-map.h			\
	gvc-channel-map.c			\
	gvc-channel-map-private.h		\
	gvc-mixer-sink.h			\
	gvc-mixer-sink.c			\
	gvc-mixer-source.h			\
	gvc-mixer-source.c			\
	gvc-mixer-sink-input.h			\
	gvc-mixer-sink-input.c			\
	gvc-mixer-source-output.h		\
	gvc-mixer-source-output.c		\
	gvc-mixer-event-role.h			\
	gvc-mixer-event-role.c			\
	gvc-mixer-control.h			\
	gvc-mixer-control.c			\
	gvc-mixer-control-private.h		\
	gvc-channel-bar.h			\
	gvc-channel-bar.c			\
	gvc-pulseaudio-fake.h			\
	$(NULL)

gnome_sound_applet_LDADD =			\
	-lm					\
	libgnomevolumecontrol.la		\
	$(SOUND_PANEL_LIBS)			\
	$(NULL)

gnome_sound_applet_SOURCES =			\
	gvc-stream-status-icon.h		\
	gvc-stream-status-icon.c		\
	gvc-applet.h				\
	gvc-applet.c				\
	gvc-log.h				\
	gvc-log.c				\
	applet-main.c				\
	$(NULL)

libsound_la_LIBADD =				\
	-lm					\
	libgnomevolumecontrol.la		\
	$(PANEL_LIBS)				\
	$(SOUND_PANEL_LIBS)			\
	$(NULL)

libsound_la_LDFLAGS =				\
	$(PANEL_LDFLAGS)			\
	$(NULL)

libsound_la_SOURCES =				\
	gvc-balance-bar.h			\
	gvc-balance-bar.c			\
	gvc-mixer-dialog.h			\
	gvc-mixer-dialog.c			\
	gvc-level-bar.h				\
	gvc-level-bar.c				\
	gvc-combo-box.h				\
	gvc-combo-box.c				\
	gvc-speaker-test.h			\
	gvc-speaker-test.c			\
	gvc-sound-theme-chooser.c		\
	gvc-sound-theme-chooser.h		\
	sound-theme-file-utils.c		\
	sound-theme-file-utils.h		\
	cc-sound-panel.c			\
	cc-sound-panel.h			\
	$(NULL)

BUILT_SOURCES =				\
	$(NULL)

CLEANFILES =				\
	$(BUILT_SOURCES)		\
	$(NULL)

MAINTAINERCLEANFILES =                  \
        *~                              \
        Makefile.in

-include $(top_srcdir)/git.mk
