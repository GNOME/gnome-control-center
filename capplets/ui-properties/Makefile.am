Applicationsdir = $(datadir)/control-center/Desktop
Applications_DATA = \
	application.desktop
	dialogs.desktop
	mdi.desktop

Gladedir = $(datadir)/control-center-data
Glade_DATA = \
	ui-properties.glade

glade_msgs = \
	ui-properties.glade.h

EXTRA_DIST = $(Applications_DATA) $(Glade_DATA) $(glade_msgs)

INCLUDES = \
        -DGNOMELOCALEDIR=\""$(datadir)/locale"\" 	\
	-DGNOME_ICONDIR=\""${prefix}/share/pixmaps"\"	\
	-DG_LOG_DOMAIN=\"ui-properties\"		\
	-DGLADE_DATADIR=\""$(Gladedir)"\"		\
	-I$(top_srcdir)/libcapplet			\
	$(GNOME_INCLUDEDIR)				\
	$(XML_CFLAGS)					\
	$(LIBGLADE_CFLAGS)				\
	-I$(top_srcdir)/				\
	-I$(top_srcdir)/intl				

bin_PROGRAMS = ui-properties-capplet

ui_properties_capplet_SOURCES = \
	prefs-widget.c prefs-widget.h \
	prefs-widget-app.c prefs-widget-app.h \
	prefs-widget-dialogs.c prefs-widget-dialogs.h \
	prefs-widget-mdi.c prefs-widget-mdi.h \
	preferences.c preferences.h \
	main.c

ui_properties_capplet_LDADD = \
	$(GNOME_LIBDIR)					\
	$(GNOMEUI_LIBS)					\
	$(ORBIT_LIBS)					\
	$(GNOME_XML_LIB)				\
	$(LIBGLADE_LIBS)				\
	-lgdk_pixbuf					\
	../../libcapplet/libcapplet.la
