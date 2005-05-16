icondir   = $(datadir)/icons/hicolor/48x48/apps
icon_DATA = gnome-control-center.png

sysdir = $(datadir)/applications
sys_in_files = gnomecc.desktop.in
sys_DATA = $(sys_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

INCLUDES = 							\
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" 	\
	-DGNOMECC_ICONS_DIR=\""$(GNOMECC_ICONS_DIR)"\"		\
	$(GNOMECC_CFLAGS)

bin_PROGRAMS = gnome-control-center

gnome_control_center_SOURCES = \
	control-center-categories.c	\
	control-center-categories.h	\
	control-center.c		\
	gnomecc-rounded-rect.c		\
	gnomecc-rounded-rect.h		\
	gnomecc-event-box.c		\
	gnomecc-event-box.h		\
	gnomecc-canvas.c		\
	gnomecc-canvas.h

gnome_control_center_LDADD = $(GNOMECC_LIBS)

IMAGES = text-selection-frame.png
VARIABLES = \
	gnomecc_rounded_rect_frame		$(srcdir)/text-selection-frame.png

pixbuf_file = gnomecc-rounded-rect-pixbuf.h

$(pixbuf_file) : $(IMAGES)
	   $(GDK_PIXBUF_CSOURCE) --raw --build-list $(VARIABLES) > $@

EXTRA_DIST = gnomecc.desktop.in	$(IMAGES) $(icon_DATA)

BUILT_SOURCES =	$(pixbuf_file)
DISTCLEANFILES = gnomecc.desktop $(pixbuf_file)
