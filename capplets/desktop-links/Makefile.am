SUBDIRS = Sawfish

ccenterdir = $(datadir)/control-center/capplets

screensaver.desktop.in: screensaver.desktop.in.in
	sed -e "s#Icon=.*#Icon=$(GNOMECC_ICONS_DIR)/screensaver-capplet.png#" < $(top_srcdir)/capplets/desktop-links/screensaver.desktop.in.in > $(top_srcdir)/capplets/desktop-links/screensaver.desktop.in

Desktopdir = $(ccenterdir)
Desktop_in_files = gnome-hint-properties.desktop.in
Desktop_in_nodist = screensaver.desktop.in
Desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop) $(Desktop_in_nodist:.desktop.in=.desktop)

iconsdir   = $(GNOMECC_ICONS_DIR)
icons_DATA = screensaver-capplet.png

# following is for control-center-plus stuff 
Advanced_capplets = cd panel legacy-applications session
Advanced_pixmaps_pre = $(Advanced_capplets:=-capplet.png)
Advanced_pixmaps_DATA = $(Advanced_pixmaps_pre) advanced-directory.png
Advanced_pixmapsdir = $(GNOMECC_ICONS_DIR)

Advanced_desktop_files = $(Advanced_capplets:=.desktop)
Advanced_desktop_in_files = $(Advanced_desktop_files:.desktop=.desktop.in)
Advanced_desktop_in_in_files = $(Advanced_desktop_files:.desktop=.desktop.in.in)

Advanced_desktop_filesdir = $(ccenterdir)/Advanced
Advanced_desktop_files_DATA = $(Advanced_desktop_files)

$(Advanced_desktop_in_files): %.desktop.in: %.desktop.in.in
	sed s#Icon=#Icon=$(GNOMECC_ICONS_DIR)/# < $< > $@

$(Advanced_desktop_files): %.desktop: %.desktop.in
	$(top_builddir)/xml-i18n-merge -d $(top_srcdir)/po $< $@

## Create the .directory file
##
## foo.directory.in is created from foo.directory.in.in and substituting the Icon field with
## the full path of the icon.
## foo.directory.in is merged with the translations to create foo.directory
## foo.directory is installed and renamed
##
groups = Advanced
groups_files = $(groups:=.directory)
groups_in_files = $(groups_files:.directory=.directory.in)
groups_in_in_files = $(groups_files:.directory=.directory.in.in)

Gnomeccdir = $(datadir)/control-center/capplets/$(groups)
Gnomecc_DATA = $(groups_files)

$(groups_in_files): %.directory.in: %.directory.in.in
	    sed s#Icon=#Icon=$(GNOMECC_ICONS_DIR)/# < $< > $@

$(groups_files): %.directory: %.directory.in
	$(top_builddir)/xml-i18n-merge -d $(top_srcdir)/po $< $@

## We need to install this ourselves cause we change the name of the
## file to .directory. 
install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(Gnomeccdirp)
	$(INSTALL_DATA) $(groups).directory $(DESTDIR)$(Gnomeccdir)/.directory

# end control-center-plus

@XML_I18N_MERGE_DESKTOP_RULE@

EXTRA_DIST = \
	$(Desktop_in_files) \
	$(Advanced_desktop_in_in_files) \
	$(groups_in_in_files) \
	$(icons_DATA)	    \
	$(Advanced_pixmaps_DATA) \
	screensaver.desktop.in.in
