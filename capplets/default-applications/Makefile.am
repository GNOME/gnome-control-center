bin_PROGRAMS = gnome-default-applications-properties

gnome_default_applications_properties_LDADD = @OLD_CAPPLET_LIBS@
gnome_default_applications_properties_SOURCES = \
	default-application-properties.c 	\
	interface.c		interface.h 	\
	callbacks.c		callbacks.h 	\
	support.c		support.h	\
	defaults.h

@INTLTOOL_DESKTOP_RULE@

iconsdir   = $(GNOMECC_ICONS_DIR)
icons_DATA = default-applications-capplet.png

desktopdir = $(GNOMECC_DESKTOP_DIR)
desktop_DATA = default-applications.desktop

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS) @OLD_CAPPLET_CFLAGS@
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(icons_DATA) $(desktop_DATA)
