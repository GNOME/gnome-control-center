appicondir = $(datadir)/control-center-2/icons
appicon_DATA = control-center.png

splashdir = $(datadir)/pixmaps/gnomecc
splash_DATA = \
	bcg_top.png \
	title.png \
	blank.png \
	active.png

sysdir = $(datadir)/applications
sys_in_files = gnomecc.desktop.in
sys_DATA = $(sys_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

gladedir = $(datadir)/control-center-2/interfaces
glade_DATA = \
	gnomecc.glade	\
	gnome-lockscreen.png	\
	gnome-unlockscreen.png	\
	html-view.xpm		\
	icon-view.xpm		\
	tree-view.xpm

INCLUDES = 							\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 		\
	-DGLADEDIR=\""$(datadir)/control-center-2/interfaces"\"	\
	-DVERSION=\""$(VERSION)"\"				\
	-DSETTINGS_DIR=\""$(datadir)/control-center-2/capplets"\"	\
	-DPIXMAPS_DIR=\""$(appicondir)"\"			\
	-DART_DIR=\""$(splashdir)"\"				\
	-DGNOME_SBINDIR=\""$(sbindir)"\"			\
	-I/gnome/head/INSTALL/include/gnome-desktop-2.0	\
	@GNOMECC_CFLAGS@	

bin_PROGRAMS = gnome2-control-center

gnome2_control_center_SOURCES = \
	main.c						\
	capplet-dir.c 		capplet-dir.h 		\
	capplet-dir-view.c	capplet-dir-view.h	\
	preferences.c		preferences.h		\
	capplet-dir-view-tree.c				\
	capplet-dir-view-list.c

gnome2_control_center_LDADD = \
	@GNOMECC_LIBS@ \
	-lgnome-desktop-2

EXTRA_DIST = gnomecc.desktop.in $(appicon_DATA) $(splash_DATA) $(glade_DATA) Gnome.directory.in.in

paneldir        = $(datadir)/gnome/apps/Settings
ccenterdir      = $(datadir)/control-center-2/capplets
ccenter_DATA = Gnome.directory
groups = Gnome
groups_files = $(groups:=.directory)
groups_in_files = $(groups_files:.directory=.directory.in)


$(groups_in_files): %.directory.in: %.directory.in.in
	    sed s#Icon=#Icon=$(GNOMECC_ICONS_DIR)/control-center.png# < $< > $@

$(groups_files): %.directory: %.directory.in
	$(top_builddir)/intltool-merge -d $(top_srcdir)/po $< $@

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(datadir)/control-center-2/capplets
	$(INSTALL_DATA) Gnome.directory $(DESTDIR)$(datadir)/control-center-2/capplets/.directory

CLEANFILES=Gnome.directory Gnome.directory.in gnomecc.desktop