## removed legacy applications for now -- grdb is not part of GNOME2
capplets = 

## Pixmaps

pixmaps_pre = $(capplets:=-capplet.png)
pixmaps_DATA = $(pixmaps_pre) accessibility-directory.png
pixmapsdir = $(datadir)/icons/gnome/48x48/apps

##
##  Desktop files
##

desktop_files = $(capplets:=.desktop)
desktop_in_files = $(desktop_files:.desktop=.desktop.in)

desktopfilesdir = $(GNOMECC_DESKTOP_DIR)
desktopfiles_DATA = $(desktop_files)

CLEANFILES = $(desktop_files) $(desktop_in_files)
EXTRA_DIST = $(desktop_in_files) $(groups_in_files) $(pixmaps_DATA)

@INTLTOOL_DESKTOP_RULE@

all-local: $(desktop_files) $(groups_files)

groups = Advanced Accessibility
groups_files = $(groups:=.directory)
groups_in_files = $(groups_files:.directory=.directory.in)

Gnomeccdir = $(datadir)/gnome/vfolders
Gnomecc_DATA = $(groups_files)

@INTLTOOL_DIRECTORY_RULE@
