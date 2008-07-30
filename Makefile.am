SUBDIRS = po libwindow-settings libslab capplets shell help

if HAVE_TYPING_BREAK
SUBDIRS += typing-break
endif

ACLOCAL_AMFLAGS = -I m4

DISTCLEANFILES = \
	gnome-doc-utils.make

EXTRA_DIST = \
	MAINTAINERS \
	gnome-doc-utils.make

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper --disable-update-mimedb
