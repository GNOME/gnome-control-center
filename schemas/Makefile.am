schemadir   = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files= \
	apps_gnome_settings_daemon_screensaver.schemas.in	\
	apps_gnome_settings_daemon_default_editor.schemas.in	\
	desktop_gnome_font_rendering.schemas.in			\
	desktop_gnome_peripherals_keyboard_xkb.schemas.in	\
	apps_gnome_settings_daemon_keybindings.schemas.in

schema_DATA = $(schema_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

if GCONF_SCHEMAS_INSTALL
# don't do this if we are building in eg. rpm
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schema_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p; \
		done \
	fi
else
install-data-local:
endif

EXTRA_DIST=$(schema_in_files)

CLEANFILES = $(schema_DATA)
