schemadir   = $(sysconfdir)/gconf/schemas
schema_DATA = peripherals.schemas


##FIXME: s/gconftool/$(GCONFTOOL) sometime
install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_CONFIG_SOURCE) gconftool --makefile-install-rule $(srcdir)/$(schema_DATA)
