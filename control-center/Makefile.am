appicondir = $(datadir)/control-center/icons
appicon_DATA = control-center.png

splashdir = $(datadir)/pixmaps/gnomecc
splash_DATA = \
	bcg_top.png \
	title.png \
	blank.png \
	active.png

sysdir = $(datadir)/gnome/apps/Settings
sys_in_files = gnomecc.desktop.in
sys_DATA = $(sys_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

gladedir = $(datadir)/control-center/interfaces
glade_DATA = \
	gnomecc.glade	\
	gnome-lockscreen.png	\
	gnome-unlockscreen.png	\
	html-view.xpm		\
	icon-view.xpm		\
	tree-view.xpm

INCLUDES = 							\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" 		\
	-DGLADEDIR=\""$(datadir)/control-center/interfaces"\"	\
	-DVERSION=\""$(VERSION)"\"				\
	-DSETTINGS_DIR=\""$(datadir)/control-center/capplets"\"	\
	-DPIXMAPS_DIR=\""$(appicondir)"\"			\
	-DART_DIR=\""$(splashdir)"\"				\
	-DGNOME_SBINDIR=\""$(sbindir)"\"			\
	-I/gnome/head/INSTALL/include/gnome-desktop-2.0	\
	@GNOMECC_CFLAGS@	

bin_PROGRAMS = gnomecc

gnomecc_SOURCES = \
	main.c						\
	capplet-dir.c 		capplet-dir.h 		\
	capplet-dir-view.c	capplet-dir-view.h	\
	preferences.c		preferences.h		\
	capplet-dir-view-tree.c				\
	capplet-dir-view-list.c

gnomecc_LDADD = \
	@GNOMECC_LIBS@ \
	-lgnome-desktop-2

EXTRA_DIST = gnomecc.desktop.in $(appicon_DATA) $(splash_DATA) $(glade_DATA) Gnome.directory.in.in

paneldir        = $(datadir)/gnome/apps/Settings
ccenterdir      = $(datadir)/control-center/capplets
ccenter_DATA = Gnome.directory
groups = Gnome
groups_files = $(groups:=.directory)
groups_in_files = $(groups_files:.directory=.directory.in)


$(groups_in_files): %.directory.in: %.directory.in.in
	    sed s#Icon=#Icon=$(GNOMECC_ICONS_DIR)/control-center.png# < $< > $@

$(groups_files): %.directory: %.directory.in
	$(top_builddir)/intltool-merge -d $(top_srcdir)/po $< $@

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(datadir)/control-center/capplets
	$(INSTALL_DATA) Gnome.directory $(DESTDIR)$(datadir)/control-center/capplets/.directory























#all-local: Gnome.directory
