SUBDIRS = control

bin_PROGRAMS = gtk-theme-selector-capplet

gtk_theme_selector_capplet_LDADD = $(GNOMECC_CAPPLETS_LIBS)
gtk_theme_selector_capplet_SOURCES = \
	da.h		\
	file.c		\
	globals.c	\
	lister.c	\
	main.c		\
	signals.c	\
	gui.c		\
	install.c

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(GNOMECC_DATA_DIR)/capplets/Advanced
desktop_DATA = gtk-theme-selector.desktop

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Glade_DATA) $(desktop_DATA)
