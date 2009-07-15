SUBDIRS = po libwindow-settings capplets font-viewer help

if HAVE_LIBSLAB_DEPS
SUBDIRS += libslab shell
endif

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

-include $(top_srcdir)/git.mk
