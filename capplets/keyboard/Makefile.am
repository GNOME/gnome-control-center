bin_PROGRAMS = gnome-keyboard-properties

gnome_keyboard_properties_SOURCES = gnome-keyboard-properties.c \
                                    gnome-keyboard-properties-xkb.c \
                                    gnome-keyboard-properties-xkbmc.c \
                                    gnome-keyboard-properties-xkblt.c \
                                    gnome-keyboard-properties-xkbot.c \
                                    gnome-keyboard-properties-xkbpv.c \
                                    gnome-keyboard-properties-xkb.h
gnome_keyboard_properties_LDADD =				\
	../accessibility/keyboard/libaccessibility-keyboard.a 	\
	$(GNOMECC_CAPPLETS_LIBS) $(LIBXKLAVIER_LIBS) \
	../../libgswitchit/libgswitchit.a \
	../../libkbdraw/libkbdraw.a

@INTLTOOL_DESKTOP_RULE@

Gladedir   = $(GNOMECC_GLADE_DIR)
Glade_DATA = gnome-keyboard-properties.glade

pixmapsdir   = $(GNOMECC_PIXMAPS_DIR)
pixmaps_DATA = \
	keyboard-repeat.png		\
	keyboard-cursor.png		\
	keyboard-volume.png		\
	keyboard-bell.png

desktopdir = $(datadir)/applications
Desktop_in_files = keyboard.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS) $(LIBXKLAVIER_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(Desktop_in_files) $(desktop_DATA)
EXTRA_DIST = $(Glade_DATA) $(pixmaps_DATA)
