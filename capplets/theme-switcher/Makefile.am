SUBDIRS = control

cappletname = gtk-theme-selector
cappletgroup = "Advanced/"
bin_PROGRAMS = gtk-theme-selector-capplet

gtk_theme_selector_capplet_LDADD = $(GNOMECC_CAPPLETS_LIBS)
gtk_theme_selector_capplet_SOURCES = \
	da.h			\
	file.c		\
	globals.c		\
	lister.c		\
	main.c		\
	signals.c		\
	gui.c		\
	install.c

pixmap_DATA = 

##
## You should not need to modify anything below this line
##
@INTLTOOL_DESKTOP_RULE@
@GNOMECC_CAPPLETS_DESKTOP_IN_RULE@

INCLUDES   = $(GNOMECC_CAPPLETS_CFLAGS)
CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES)
EXTRA_DIST = $(GNOMECC_CAPPLETS_EXTRA_DIST)
iconsdir   = $(GNOMECC_ICONS_DIR)
Gladedir   = $(GNOMECC_GLADE_DIR)
pixmapdir  = $(GNOMECC_PIXMAPS_DIR)
Glade_DATA = $(cappletname)-properties.glade
icons_DATA = $(cappletname)-capplet.png
desktop    = $(cappletname).desktop
all-local: $(desktop)
