ccenterdir = $(datadir)/control-center/capplets

Desktopdir = $(ccenterdir)
Desktop_in_files = gnome-hint-properties.desktop.in
Desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop) 
@XML_I18N_MERGE_DESKTOP_RULE@

SUBDIRS = Sawfish

EXTRA_DIST = \
	$(Desktop_in_files) 	
