schemadir   = $(GCONF_SCHEMA_FILE_DIR)
schema_DATA = \
	gnome-control-center-1.6_background-properties.schemas \
	gnome-control-center-1.6_keyboard-properties.schemas \
	gnome-control-center-1.6_mouse-properties.schemas \
	gnome-control-center-1.6_sound-properties.schemas 

# don't do this if we are building in eg. rpm
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schema_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $(srcdir)/$$p; \
		done \
	fi

EXTRA_DIST = $(schema_DATA)
