INCLUDES =						\
	@GNOME_CFLAGS@					\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DIMAGEDIR=\"$(GNOMECC_PIXMAPS_DIR)\"

bin_PROGRAMS = gnome-typing-monitor

gnome_typing_monitor_SOURCES =			\
	main.c					\
	drwright.c				\
	drwright.h				\
	drw-break-window.c			\
	drw-break-window.h			\
	drw-monitor.c				\
	drw-monitor.h				\
	drw-utils.c				\
	drw-utils.h				\
	drw-selection.c				\
	drw-selection.h				\
	eggtrayicon.c				\
	eggtrayicon.h				\
	egg-spawn.c				\
	egg-spawn.h

gnome_typing_monitor_LDADD = @GNOME_LIBS@ -L/usr/X11R6/lib -lXss
gnome_typing_monitor_LDFLAGS = -export-dynamic

imagedir = $(GNOMECC_PIXMAPS_DIR)
image_DATA = stop.png bar.png bar-red.png bar-green.png bar-disabled.png ocean-stripes.png

EXTRA_DIST = $(image_DATA)
