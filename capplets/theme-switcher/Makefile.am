INCLUDES = -I. -I$(srcdir) 				\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	-I$(srcdir)/../../control-center		\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(includedir) $(GNOME_INCLUDEDIR)

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

theme_selector_capplet_LDFLAGS = ../../control-center/libcapplet.la `imlib-config --libs`

theme_selector_capplet_LDADD = ../../control-center/libcapplet.la \
	$(GNOME_LIBDIR) $(ORB_LIBS) \
	$(GNOMEUI_LIBS) $(INTLLIBS) \
	-lImlib  $(GTKXMHTML_LIBS) -lgnorba

EXTRA_DIST = \
	theme-selector.desktop

sysdir = $(datadir)/control-center/Desktop
sys_DATA = theme-selector.desktop

install-data-local:
	$(mkinstalldirs) $(datadir)/gnome/apps/control-center/Desktop
	$(INSTALL_DATA) $(srcdir)/theme-selector.desktop $(datadir)/gnome/apps/control-center/Desktop/theme-selector.desktop
