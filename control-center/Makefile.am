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

serverdir = $(libdir)/bonobo/servers
server_in_files = GNOME_ControlCenter.server.in
server_DATA = $(server_in_files:.server.in=.server)
@INTLTOOL_SERVER_RULE@

uidir = $(datadir)/gnome-2.0/ui
ui_DATA = gnomecc-ui.xml

INCLUDES = 							\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DSETTINGS_DIR=\""$(GNOMECC_DESKTOP_DIR)"\"		\
	-DGNOMECC_ICONS_DIR=\""$(GNOMECC_ICONS_DIR)"\"		\
	-DPIXMAP_DIR=\""$(datadir)/pixmaps"\"			\
	-DART_DIR=\""$(splashdir)"\"				\
	-DGNOME_SBINDIR=\""$(sbindir)"\"			\
	$(GNOMECC_CFLAGS) $(NAUTILUS_CFLAGS)

bin_PROGRAMS = gnome-control-center

gnome_control_center_SOURCES = \
	control-center-categories.c	\
	control-center-categories.h	\
	control-center.c		\
	gnomecc-event-box.c		\
	gnomecc-event-box.h

gnome_control_center_LDADD = $(GNOMECC_LIBS) $(NAUTILUS_LIBS)

EXTRA_DIST = gnomecc.desktop.in $(splash_DATA) $(ui_DATA) $(server_DATA)

CLEANFILES = gnomecc.desktop
