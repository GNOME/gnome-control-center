appicondir = $(datadir)/pixmaps
appicon_DATA = control-center.png

splashdir = $(datadir)/pixmaps/gnomecc
splash_DATA = \
	ccsplash.png	\
	bgtop.png	\
	bg.png		\
	empty.png	\
	foot.png	\
	left_top.png	\
	left.png

sysdir = $(datadir)/gnome/apps/Settings
sys_DATA = gnomecc.desktop

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

EXTRA_DIST = gnomecc.desktop $(appicon_DATA) $(splash_DATA) $(glade_DATA)
