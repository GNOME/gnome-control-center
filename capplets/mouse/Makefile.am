@XML_I18N_MERGE_DESKTOP_RULE@
@GNOMECC_CAPPLETS_DESKTOP_IN_RULE@

cappletname = mouse

INCLUDES = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(GNOMECC_CAPPLETS_EXTRA_DIST)

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = \
	mouse-left.png	\
	mouse-right.png

bin_PROGRAMS = mouse-properties-capplet

mouse_properties_capplet_LDADD = @CAPPLET_LIBS@
mouse_properties_capplet_SOURCES = \
	main.c\
	prefs-widget.c		prefs-widget.h \
	preferences.c		preferences.h

desktop = $(cappletname).desktop

all-local: $(desktop)
