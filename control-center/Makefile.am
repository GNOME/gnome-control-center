appicondir = $(datadir)/pixmaps
appicon_DATA = control-center.png

splashdir = $(datadir)/pixmaps
splash_DATA = ccsplash.png

sysdir = $(datadir)/gnome/apps/Settings
sys_DATA = gnomecc.desktop

INCLUDES = 			\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	-I$(includedir) $(GNOME_INCLUDEDIR)		\
	-DVERSION=\""$(VERSION)"\"			\
	-DSETTINGS_DIR=\""$(sysdir)"\"			\
	-DPIXMAPS_DIR=\""$(appicondir)"\"

bin_PROGRAMS = gnomecc root-manager

gnomecc_SOURCES = \
	main.c						\
	capplet-dir.c 		capplet-dir.h 		\
	capplet-dir-window.c 	capplet-dir-window.h

gnomecc_LDADD = \
	$(GNOME_LIBDIR)					\
	$(GNOMEUI_LIBS)					\
	$(INTLLIBS)

root_manager_SOURCES = \
	root-manager.c

root_manager_LDADD = \
	$(GNOME_LIBS)

EXTRA_DIST = gnomecc.desktop $(appicon_DATA) $(splash_DATA)
