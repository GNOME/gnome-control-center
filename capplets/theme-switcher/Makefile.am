INCLUDES = -I. -I$(srcdir) 				\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	-I$(srcdir)/../../libcapplet		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	@CAPPLET_CFLAGS@

bin_PROGRAMS = gtk-theme-selector-capplet

gtk_theme_selector_capplet_SOURCES = \
        da.h		\
	file.c		\
	globals.c	\
	lister.c        \
	main.c          \
	signals.c       \
	demo.c          \
	gui.c           \
	install.c

gtk_theme_selector_capplet_LDADD = @CAPPLET_LIBS@

EXTRA_DIST = \
	gtk-theme-selector.desktop

sysdir = $(datadir)/control-center/capplets/Advanced
sys_DATA = gtk-theme-selector.desktop

psysdir = $(datadir)/gnome/apps/Settings/Advanced
psys_DATA = gtk-theme-selector.desktop
