Applicationsdir = $(datadir)/control-center/Desktop
Applications_DATA = \
	mouse-properties.desktop

Gladedir = $(datadir)/control-center-data
Glade_DATA = \
	mouse-properties.glade

EXTRA_DIST = ChangeLog $(Applications_DATA) $(Glade_DATA)

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\"	\
	-DG_LOG_DOMAIN=\"mouse-properties\"		\
	-DGLADE_DATADIR=\""$(Gladedir)"\"		\
	$(GNOME_INCLUDEDIR)				\
	$(XML_CFLAGS)					\
	$(LIBGLADE_CFLAGS)				\
	-I$(top_srcdir)/				\
	-I$(top_srcdir)/intl				\
	-I$(top_srcdir)/libcapplet

bin_PROGRAMS = mouse-properties-capplet

mouse_properties_capplet_SOURCES = \
	prefs-widget.c prefs-widget.h \
	preferences.c preferences.h \
	main.c

mouse_properties_capplet_LDADD = \
	$(GNOME_LIBDIR)					\
	$(GNOMEUI_LIBS)					\
	$(ORBIT_LIBS)					\
	$(GNOME_XML_LIB)				\
	$(LIBGLADE_LIBS)				\
	-lgdk_pixbuf					\
	../../libcapplet/libcapplet.la
