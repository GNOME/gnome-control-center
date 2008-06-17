SUBDIRS = po libsounds libwindow-settings libslab capplets shell help vfs-methods $(TYPING_BREAK)

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
