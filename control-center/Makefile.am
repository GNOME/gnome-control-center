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
@XML_I18N_MERGE_DESKTOP_RULE@

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
	-I$(top_srcdir)/intl -I$(top_builddir)/intl		\
	-DVERSION=\""$(VERSION)"\"				\
	-DSETTINGS_DIR=\""$(datadir)/control-center/capplets"\"	\
	-DPIXMAPS_DIR=\""$(appicondir)"\"			\
	-DART_DIR=\""$(splashdir)"\"				\
	-DGNOME_SBINDIR=\""$(sbindir)"\"			\
	@GNOMECC_CFLAGS@

bin_PROGRAMS = gnomecc

gnomecc_SOURCES = \
	main.c						\
	capplet-dir.c 		capplet-dir.h 		\
	capplet-dir-view.c	capplet-dir-view.h	\
	preferences.c		preferences.h		\
	capplet-dir-view-html.c				\
	capplet-dir-view-tree.c				\
	capplet-dir-view-list.c

gnomecc_LDADD = \
	@GNOMECC_LIBS@					\
	$(INTLLIBS)

EXTRA_DIST = gnomecc.desktop.in Gnome.directory.in.in $(appicon_DATA) $(splash_DATA) $(glade_DATA) $(groups_in_files)

paneldir        = $(datadir)/gnome/apps/Settings
ccenterdir      = $(datadir)/control-center/capplets
ccenter_DATA = Gnome.directory
groups = Gnome
groups_files = $(groups:=.directory)
groups_in_files = $(groups_files:.directory=.directory.in)

$(groups_in_files): %.directory.in: %.directory.in.in
	    sed s#Icon=#Icon=$(GNOMECC_ICONS_DIR)/control-center.png# < $< > $@

$(groups_files): %.directory: %.directory.in
	$(top_builddir)/xml-i18n-merge -d $(top_srcdir)/po $< $@

install-data-local:
	$(mkinstalldirs) $(datadir)/control-center/capplets/$(cappletgroup)
	$(INSTALL_DATA) $(srcdir)/$(groups).directory $(datadir)/control-center/capplets/.directory
