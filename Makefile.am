SUBDIRS = libsounds libbackground gnome-settings-daemon capplets control-center idl po

#I removed the root-manager for now
#
# it generated this error :
# root-manager.c:55:31: security/pam_appl.h: No such file or directory
# while compiling. I guess we need to check for pam or something
# in the configure.in file. Since we are not using it now, i will
# remove it from the default build. Chema

pixmapdir = $(datadir)/pixmaps
pixmap_DATA = control-center.png
 
dist-hook: control-center.spec
	cp control-center.spec $(distdir)

CLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update

EXTRA_DIST = \
	control-center.spec.in	\
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	control-center.png
