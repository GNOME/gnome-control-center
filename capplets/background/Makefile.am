Applicationsdir = $(datadir)/control-center/Desktop
Applications_DATA = \
	background-properties.desktop

Gladedir = $(datadir)/control-center-data
Glade_DATA = \
	background-properties.glade

EXTRA_DIST = ChangeLog $(Applications_DATA) $(Glade_DATA)

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\"	\
	-DG_LOG_DOMAIN=\"background-properties\"	\
	-DGLADE_DATADIR=\""$(Gladedir)"\"		\
	$(GNOME_INCLUDEDIR)				\
	$(LIBGLADE_CFLAGS)				\
	$(XML_CFLAGS)					\
	-I$(top_srcdir)/				\
	-I$(top_srcdir)/intl				\
	-I$(top_srcdir)/libcapplet

bin_PROGRAMS = background-properties-capplet

background_properties_capplet_SOURCES = \
	prefs-widget.c prefs-widget.h 			\
	preferences.c preferences.h 			\
	applier.c applier.h 				\
	main.c

background_properties_capplet_LDADD = \
	$(GNOME_LIBDIR)					\
	$(GNOMEUI_LIBS)					\
	$(ORBIT_LIBS)					\
	$(GNOME_XML_LIB)				\
	$(LIBGLADE_LIBS)				\
	-lgdk_pixbuf					\
	-lgdk_pixbuf_xlib				\
	../../libcapplet/libcapplet.la
