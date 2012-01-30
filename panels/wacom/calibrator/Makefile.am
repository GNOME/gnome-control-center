# This is used in PANEL_CFLAGS
cappletname = wacom

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(WACOM_PANEL_CFLAGS)				\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	-DPIXMAP_DIR=\""$(datadir)/gnome-control-center/pixmaps"\"	\
	$(NULL)


noinst_LTLIBRARIES = libwacom-calibrator.la

libwacom_calibrator_la_SOURCES =	\
	calibrator.c			\
	calibrator.h			\
	gui_gtk.c			\
	gui_gtk.h

libwacom_calibrator_la_LIBADD = $(PANEL_LIBS) $(WACOM_PANEL_LIBS)
libwacom_calibrator_la_LDFLAGS = $(PANEL_LDFLAGS)

noinst_PROGRAMS = test-calibrator

test_calibrator_SOURCES =		\
	main.c				\
	calibrator.c			\
	calibrator.h			\
	gui_gtk.c			\
	gui_gtk.h

test_calibrator_CPPFLAGS = $(INCLUDES)
test_calibrator_LDADD = $(PANEL_LIBS) $(WACOM_PANEL_LIBS)

-include $(top_srcdir)/git.mk
