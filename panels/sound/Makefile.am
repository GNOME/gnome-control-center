SUBDIRS = data gvc

# This is used in PANEL_CFLAGS
cappletname = sound
NULL =

noinst_LTLIBRARIES = libsound.la libgvcgtk.la

AM_CPPFLAGS =					\
	-I$(srcdir)/gvc/			\
	$(PANEL_CFLAGS) 			\
	$(SOUND_PANEL_CFLAGS)			\
	-DLOCALE_DIR=\""$(datadir)/locale"\"	\
	-DLIBEXECDIR=\"$(libexecdir)\"		\
	-DGLADEDIR=\""$(pkgdatadir)"\"		\
        -DSOUND_DATA_DIR="\"$(datadir)/sounds\""	\
        -DSOUND_SET_DIR="\"$(pkgdatadir)/sounds\""	\
	-DICON_DATA_DIR="\"$(pkgdatadir)/icons\"" \
	$(NULL)

# Needed because Automake complains if
# the same source file is used in a library
# and in an executable
libgvcgtk_la_SOURCES = 				\
	gvc-channel-bar.h			\
	gvc-channel-bar.c			\
	$(NULL)

libsound_la_LIBADD =				\
	gvc/libgvc.la				\
	libgvcgtk.la				\
	$(PANEL_LIBS)				\
	$(SOUND_PANEL_LIBS)			\
	$(LIBM)					\
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
