bin_PROGRAMS=gnome-appearance-properties

gnome_appearance_properties_SOURCES= \
	appearance.h \
	appearance-main.c \
	appearance-themes.c \
	appearance-themes.h \
	appearance-ui.c \
	appearance-ui.h

gnome_appearance_properties_LDADD = \
	$(GNOMECC_CAPPLETS_LIBS) 					\
	$(METACITY_LIBS)						\
	$(top_builddir)/libwindow-settings/libgnome-window-settings.la  \
	$(top_builddir)/capplets/common/libcommon.la

gladedir = $(pkgdatadir)/glade
glade_DATA = appearance.glade

INCLUDES   = 				\
	-I$(top_srcdir)			\
	$(METACITY_CFLAGS)		\
	$(GNOMECC_CAPPLETS_CFLAGS)	\
	-DGNOMECC_GLADE_DIR=\""$(gladedir)"\"	\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""

EXTRA_DIST = $(glade_DATA)
