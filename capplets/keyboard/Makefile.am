@XML_I18N_MERGE_DESKTOP_RULE@
@GNOMECC_CAPPLETS_DESKTOP_IN_RULE@

cappletname = keyboard

INCLUDES = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(GNOMECC_CAPPLETS_EXTRA_DIST)

bin_PROGRAMS = keyboard-properties

keyboard_properties_LDADD = @CAPPLET_LIBS@ $(XF86MISC_LIBS)
keyboard_properties_SOURCES = \
	main.c \
	prefs-widget.c		prefs-widget.h \
	preferences.c		preferences.h

desktop = $(cappletname).desktop

all-local: $(desktop)

