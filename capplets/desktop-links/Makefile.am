ccenterdir = $(datadir)/control-center/capplets

screensaver.desktop.in: screensaver.desktop.in.in
	sed -e "s#Icon=.*#Icon=$(GNOMECC_ICONS_DIR)/screensaver-capplet.png#" < $(top_srcdir)/capplets/desktop-links/screensaver.desktop.in.in > $(top_srcdir)/capplets/desktop-links/screensaver.desktop.in

Desktopdir = $(ccenterdir)
Desktop_in_files = gnome-hint-properties.desktop.in
Desktop_in_nodist = screensaver.desktop.in
Desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop) $(Desktop_in_nodist:.desktop.in=.desktop)

@XML_I18N_MERGE_DESKTOP_RULE@

iconsdir   = $(GNOMECC_ICONS_DIR)
icons_DATA = screensaver-capplet.png

EXTRA_DIST = \
	$(Desktop_in_files) \
	$(icons_DATA)	    \
	screensaver.desktop.in.in
