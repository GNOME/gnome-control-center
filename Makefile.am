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

MAINTAINERCLEANFILES = \
	$(srcdir)/INSTALL \
	$(srcdir)/aclocal.m4 \
	$(srcdir)/autoscan.log \
	$(srcdir)/compile \
	$(srcdir)/config.guess \
	$(srcdir)/config.h.in \
	$(srcdir)/config.sub \
	$(srcdir)/configure.scan \
	$(srcdir)/depcomp \
	$(srcdir)/install-sh \
	$(srcdir)/ltmain.sh \
	$(srcdir)/missing \
	$(srcdir)/mkinstalldirs \
	$(srcdir)/omf.make \
	$(srcdir)/xmldocs.make \
	$(srcdir)/gtk-doc.make \
	`find "$(srcdir)" -type f -name Makefile.in -print`

EXTRA_DIST = \
	MAINTAINERS \
	gnome-doc-utils.make

DISTCHECK_CONFIGURE_FLAGS = --disable-scrollkeeper --disable-update-mimedb

GITIGNOREFILES=m4

-include $(top_srcdir)/git.mk
