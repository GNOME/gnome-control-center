INCLUDES = -I. -I$(srcdir) 				\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	-I$(srcdir)/../../libcapplet		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	$(CAPPLET_CFLAGS)

bin_PROGRAMS = theme-selector-capplet

theme_selector_capplet_SOURCES = \
        da.h		\
	file.c		\
	globals.c	\
	lister.c        \
	main.c          \
	signals.c       \
	demo.c          \
	gui.c           \
	install.c

theme_selector_capplet_LDADD = $(CAPPLET_LIBS)

EXTRA_DIST = \
	theme-selector.desktop

sysdir = $(datadir)/control-center/Desktop
sys_DATA = theme-selector.desktop

psysdir = $(datadir)/gnome/apps/Settings/Desktop
psys_DATA = theme-selector.desktop
