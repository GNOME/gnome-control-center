## .desktop files
##
## translate foo.desktop.in and create a foo.desktop file.
##

capplets = \
	appearance-properties \
	misc-properties \
	bindings-properties \
	move-properties \
	focus-properties \
	placement-properties \
	match-properties \
	sound-properties \
	maximize-properties \
	workspace-properties \
	meta-properties

##
## Pixmaps
##

pixmaps_DATA = $(group_icon)
pixmapsdir = $(GNOMECC_DATA_DIR)/icons


##
## dekstop files
##
desktop_files = $(capplets:=.desktop)
desktop_in_files = $(desktop_files:.desktop=.desktop.in)

cappletsdir = $(datadir)/control-center/capplets/Sawfish
capplets_DATA = $(desktop_files)

EXTRA_DIST = $(desktop_in_files) Sawfish.directory.in.in $(pixmaps_DATA)

$(desktop_files): %.desktop: %.desktop.in
	$(top_builddir)/intltool-merge -d $(top_srcdir)/po $< $@


## Create the .directory file
##
## foo.directory.in is created from foo.directory.in.in and substituting the Icon field with
## the full path of the icon.
## foo.directory.in is merged with the translations to create foo.directory
## foo.directory is installed and renamed
##
groups = Sawfish
group_icon = sawfish-group.png
groups_files = $(groups:=.directory)
groups_in_files = $(groups_files:.directory=.directory.in)

Gnomeccdir = $(datadir)/control-center/capplets/$(groups)
Gnomecc_DATA = $(groups_files)

$(groups_in_files): %.directory.in: %.directory.in.in
	    sed s#Icon=\.*#Icon=$(GNOMECC_ICONS_DIR)/${group_icon}# < $< > $@

$(groups_files): %.directory: %.directory.in
	$(top_builddir)/intltool-merge -d $(top_srcdir)/po $< $@

## We need to install this ourselves cause we change the name of the
## file to .directory. 
install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(Gnomeccdirp)
	$(INSTALL_DATA) Sawfish.directory $(DESTDIR)$(Gnomeccdir)/.directory

