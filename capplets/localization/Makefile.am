# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = localization
bin_PROGRAMS = gnome-localization-properties

gnome_localization_properties_SOURCES =	main.c
gnome_localization_properties_LDFLAGS = -export-dynamic
gnome_localization_properties_LDADD =	\
	$(CAPPLET_LIBS)

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
Desktop_in_files = localization.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

uidir = $(pkgdatadir)/ui
dist_ui_DATA = localization.ui

INCLUDES =						\
	$(CAPPLET_CFLAGS)				\
	$(GNOMECC_CAPPLETS_CFLAGS)			\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\"" 	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""

CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(Desktop_in_files) $(desktop_DATA)
-include $(top_srcdir)/git.mk
