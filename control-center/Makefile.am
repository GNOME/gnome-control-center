splashdir = $(datadir)/pixmaps/gnomecc-2
splash_DATA = \
	bcg_top.png \
	title.png \
	blank.png \
	active.png

sysdir = $(datadir)/applications
sys_in_files = gnomecc.desktop.in
sys_DATA = $(sys_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

uidir = $(datadir)/gnome-2.0/ui
ui_DATA = gnomecc-ui.xml

INCLUDES = 							\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DSETTINGS_DIR=\""$(GNOMECC_DESKTOP_DIR)"\"		\
	-DGNOMECC_ICONS_DIR=\""$(GNOMECC_ICONS_DIR)"\"		\
	-DPIXMAP_DIR=\""$(datadir)/pixmaps"\"			\
	-DART_DIR=\""$(splashdir)"\"				\
	-DGNOME_SBINDIR=\""$(sbindir)"\"			\
	$(GNOMECC_CFLAGS)

bin_PROGRAMS = gnome-control-center

gnome_control_center_SOURCES = \
	main.c						\
	capplet-dir.c 		capplet-dir.h 		\
	capplet-dir-view.c	capplet-dir-view.h	\
	capplet-dir-view-list.c

gnome_control_center_LDADD = $(GNOMECC_LIBS)

EXTRA_DIST = gnomecc.desktop.in $(appicon_DATA) $(splash_DATA) $(ui_DATA)

CLEANFILES = gnomecc.desktop
