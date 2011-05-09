INCLUDES = $(SETUP_CFLAGS)

LIBS = $(SETUP_LIBS)

bin_PROGRAMS = gnome-setup-assistant

gnome_setup_assistant_sources =	\
	gnome-setup-assistant.c

AM_CPPFLAGS = \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
	-DUIDIR="\"$(uidir)\""

gsettings_SCHEMAS = org.gnome.control-center.setup.gschema.xml

@GSETTINGS_RULES@

uidir = $(pkgdatadir)/ui/setup
ui_DATA = \
	setup.ui
	welcome-image.jpg

sysdir = $(datadir)/applications
sys_DATA = gnome-setup-assistant.desktop

EXTRA_DIST = \
	$(ui_DATA) \
	$(sys_DATA) \
	org.gnome.control-center.setup.gschema.xml

-include $(top_srcdir)/git.mk
