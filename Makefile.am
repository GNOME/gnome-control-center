SUBDIRS = po libsounds libbackground libwindow-settings libslab gnome-settings-daemon capplets shell help vfs-methods schemas $(TYPING_BREAK)

ACLOCAL_AMFLAGS = -I m4

DISTCLEANFILES = \
	gnome-doc-utils.make \
	intltool-extract \
	intltool-merge \
	intltool-update

EXTRA_DIST = \
	MAINTAINERS \
	gnome-doc-utils.make \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper --disable-update-mimedb
