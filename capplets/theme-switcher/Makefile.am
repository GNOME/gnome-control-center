SUBDIRS = control

bin_PROGRAMS = gtk2-theme-selector-capplet

gtk2_theme_selector_capplet_LDADD = $(GNOMECC_CAPPLETS_LIBS)
gtk2_theme_selector_capplet_SOURCES = \
	main.c		
gtk2_theme_selector_capplet_LDFLAGS = -export-dynamic

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(GNOMECC_DATA_DIR)/capplets/Advanced
Desktop_in_files = gtk-theme-selector.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

gladedir = $(GNOMECC_DATA_DIR)/interfaces
glade_DATA = gtk-theme-selector.glade

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(Desktop_in_files) $(glade_DATA)
