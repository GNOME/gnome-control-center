SUBDIRS = Sawfish

## removed legacy applications for now -- grdb is not part of GNOME2
capplets = cd

## Pixmaps

pixmaps_pre = $(capplets:=-capplet.png)
pixmaps_DATA = $(pixmaps_pre) advanced-directory.png
pixmapsdir = $(GNOMECC_ICONS_DIR)

##
##  Desktop files
##

desktop_files = $(capplets:=.desktop)
desktop_in_files = $(desktop_files:.desktop=.desktop.in)
desktop_in_in_files = $(desktop_files:.desktop=.desktop.in.in)

desktopfilesdir = $(GNOMECC_DESKTOP_DIR)
desktopfiles_DATA = $(desktop_files)

CLEANFILES = $(desktop_files) $(desktop_in_files)
EXTRA_DIST = $(desktop_in_in_files) $(groups_in_in_files) $(pixmaps_DATA)

$(desktop_in_files): %.desktop.in: %.desktop.in.in
	    sed s#Icon=#Icon=$(GNOMECC_ICONS_DIR)/# < $< > $@

$(desktop_files): %.desktop: %.desktop.in
	$(top_builddir)/intltool-merge -d $(top_srcdir)/po $< $@

all-local: $(desktop_files) $(groups_files)



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

Gnomeccdir = $(datadir)/gnome/vfolders
Gnomecc_DATA = $(groups_files)

$(groups_in_files): %.directory.in: %.directory.in.in
	    sed s#Icon=#Icon=$(pixmapsdir)/# < $< > $@

$(groups_files): %.directory: %.directory.in
	$(top_builddir)/intltool-merge -d $(top_srcdir)/po $< $@
