SUBDIRS = data

# This is used in PANEL_CFLAGS
cappletname = user-accounts
NULL =

noinst_LTLIBRARIES = libuser-accounts.la

AM_CPPFLAGS =						\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-DUM_PIXMAP_DIR=\""$(pkgdatadir)/pixmaps"\"	\
	-DHAVE_LIBPWQUALITY				\
	-I$(srcdir)/../common/				\
	-I$(srcdir)/../../shell/			\
	-I$(top_srcdir)/libgd/				\
	$(PANEL_CFLAGS)					\
	$(USER_ACCOUNTS_PANEL_CFLAGS)

if BUILD_CHEESE
AM_CPPFLAGS += $(CHEESE_CFLAGS)
endif

BUILT_SOURCES = \
	um-realm-generated.c \
	um-realm-generated.h \
	um-resources.c \
	um-resources.h

libuser_accounts_la_SOURCES =		\
	um-account-type.h		\
	um-account-type.c 		\
	um-account-dialog.h		\
	um-account-dialog.c		\
	um-carousel.h			\
	um-carousel.c			\
	um-password-dialog.h		\
	um-password-dialog.c		\
	pw-utils.h			\
	pw-utils.c			\
	um-photo-dialog.h		\
	um-photo-dialog.c		\
	cc-crop-area.h			\
	cc-crop-area.c			\
	um-fingerprint-dialog.h		\
	um-fingerprint-dialog.c		\
	um-utils.h			\
	um-utils.c			\
	fingerprint-strings.h		\
	run-passwd.h			\
	run-passwd.c			\
	um-user-panel.h 		\
	um-user-panel.c			\
	um-realm-manager.c		\
	um-realm-manager.h		\
	um-history-dialog.h		\
	um-history-dialog.c		\
	um-user-image.h			\
	um-user-image.c			\
	um-cell-renderer-user-image.h	\
	um-cell-renderer-user-image.c	\
	$(BUILT_SOURCES)

libuser_accounts_la_LIBADD = 		\
	$(PANEL_LIBS)			\
	$(USER_ACCOUNTS_PANEL_LIBS)	\
	$(builddir)/../common/liblanguage.la	\
	-lpwquality			\
	-lcrypt				\
	$(LIBM)				\
	$(top_builddir)/libgd/libgd.la

if BUILD_CHEESE
libuser_accounts_la_LIBADD += $(CHEESE_LIBS)
endif

um-realm-generated.c: $(srcdir)/data/org.freedesktop.realmd.xml
	$(AM_V_GEN) gdbus-codegen --interface-prefix org.freedesktop.realmd. \
		--generate-c-code um-realm-generated --c-namespace UmRealm \
		--annotate "org.freedesktop.realmd.Realm" org.gtk.GDBus.C.Name Common \
		--c-generate-object-manager $<
um-realm-generated.h: um-realm-generated.c

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/user-accounts.gresource.xml)
um-resources.c: user-accounts.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name um $<
um-resources.h: user-accounts.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name um $<

noinst_PROGRAMS = frob-account-dialog

frob_account_dialog_SOURCES = \
	frob-account-dialog.c \
	um-account-dialog.h \
	um-account-dialog.c \
	um-realm-manager.c \
	um-realm-manager.h \
	um-utils.h \
	um-utils.c \
	pw-utils.h \
	pw-utils.c \
	$(BUILT_SOURCES)

frob_account_dialog_LDADD = \
	$(libuser_accounts_la_LIBADD)

frob_account_dialog_CFLAGS = \
	$(AM_CFLAGS)

polkitdir = $(datadir)/polkit-1/actions
polkit_in_files = org.gnome.controlcenter.user-accounts.policy.in

@INTLTOOL_POLICY_RULE@
polkit_DATA = $(polkit_in_files:.policy.in=.policy)

EXTRA_DIST =				\
	$(polkit_in_files)		\
	$(resource_files)		\
	user-accounts.gresource.xml	\
	$(NULL)

CLEANFILES =				\
	$(BUILT_SOURCES)		\
	$(polkit_DATA)			\
	$(NULL)

MAINTAINERCLEANFILES =                  \
        *~                              \
        Makefile.in

-include $(top_srcdir)/git.mk
