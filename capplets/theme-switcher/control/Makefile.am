bin_PROGRAMS = gnome-theme-preview 

INCLUDES = $(GNOMECC_CAPPLETS_CFLAGS)

gnome_theme_preview_SOURCES =			\
	control.c

gnome_theme_preview_LDADD =			\
	$(GNOMECC_CAPPLETS_LIBS)

serverdir = $(libdir)/bonobo/servers
server_in_files = GNOME_Theme_Preview.server.in
server_DATA = $(server_in_files:.server.in=.server)

@INTLTOOL_SERVER_RULE@

EXTRA_DIST = $(server_in_files) $(server_DATA)
