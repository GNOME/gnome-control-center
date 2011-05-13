INCLUDES = \
	$(SETUP_CFLAGS)	\
	$(NETWORK_MANAGER_CFLAGS)

LIBS = \
	$(SETUP_LIBS)	\
	$(NETWORK_MANAGER_LIBS)

bin_PROGRAMS = gnome-setup-assistant

gnome_setup_assistant_SOURCES =	\
	gnome-setup-assistant.c \
	panel-cell-renderer-signal.c panel-cell-renderer-signal.h \
	panel-cell-renderer-mode.c panel-cell-renderer-mode.h \
	panel-cell-renderer-security.c panel-cell-renderer-security.h \
	cc-timezone-map.c cc-timezone-map.h \
	set-timezone.c set-timezone.h \
	tz.c tz.h

AM_CPPFLAGS = \
	-DGNOMELOCALEDIR="\"$(datadir)/locale\"" \
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\"" \
	-DUIDIR="\"$(uidir)\"" \
	-DDATADIR="\"$(pkgdatadir)/ui/datetime\""

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
