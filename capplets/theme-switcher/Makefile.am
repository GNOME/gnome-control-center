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
Desktop_in_files = gtk-theme-selector.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Desktop_in_files)
