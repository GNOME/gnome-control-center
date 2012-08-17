SUBDIRS = data

# This is used in PANEL_CFLAGS
cappletname = user-accounts
NULL =

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libuser-accounts.la

AM_CPPFLAGS =						\
	-DDATADIR=\""$(datadir)"\"			\
	-DUIDIR=\""$(pkgdatadir)/ui/user-accounts"\"	\
	-DLIBLOCALEDIR=\""$(prefix)/lib/locale"\"       \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-DUM_PIXMAP_DIR=\""$(pkgdatadir)/pixmaps"\"	\
	-DHAVE_LIBPWQUALITY				\
	-I$(srcdir)/../common/				\
	-I$(srcdir)/../../shell/			\
	$(PANEL_CFLAGS)					\
	$(USER_ACCOUNTS_PANEL_CFLAGS)

if BUILD_CHEESE
AM_CPPFLAGS += $(CHEESE_CFLAGS)
endif

BUILT_SOURCES = \
       um-realm-generated.c \
       um-realm-generated.h

libuser_accounts_la_SOURCES =		\
	um-account-type.h		\
	um-account-type.c 		\
	um-user.h 			\
	um-user.c 			\
	um-user-manager.h 		\
	um-user-manager.c 		\
	um-account-dialog.h		\
	um-account-dialog.c		\
	um-password-dialog.h		\
	um-password-dialog.c		\
	pw-utils.h			\
	pw-utils.c			\
	um-photo-dialog.h		\
	um-photo-dialog.c		\
	um-crop-area.h			\
	um-crop-area.c			\
	um-fingerprint-dialog.h		\
	um-fingerprint-dialog.c		\
	um-utils.h			\
	um-utils.c			\
	fingerprint-strings.h		\
	run-passwd.h			\
	run-passwd.c			\
	um-editable-button.h		\
	um-editable-button.c		\
	um-editable-combo.h		\
	um-editable-combo.c		\
	um-user-panel.h 		\
	um-user-panel.c			\
	um-user-module.c		\
	um-realm-manager.c		\
	um-realm-manager.h		\
	$(BUILT_SOURCES)

libuser_accounts_la_LIBADD = 		\
	$(PANEL_LIBS)			\
	$(USER_ACCOUNTS_PANEL_LIBS)	\
	$(top_builddir)/panels/common/liblanguage.la \
	-lpwquality			\
	-lcrypt				\
	-lm

if BUILD_CHEESE
libuser_accounts_la_LIBADD += $(CHEESE_LIBS)
endif

libuser_accounts_la_LDFLAGS = $(PANEL_LDFLAGS)

um-realm-generated.c: $(srcdir)/data/org.freedesktop.realmd.xml
	$(AM_V_GEN) gdbus-codegen --interface-prefix org.freedesktop.realmd. \
		--generate-c-code um-realm-generated --c-namespace UmRealm \
		--annotate "org.freedesktop.realmd.Realm" org.gtk.GDBus.C.Name Common \
		--c-generate-object-manager $<
um-realm-generated.h: um-realm-generated.c

noinst_PROGRAMS = frob-account-dialog

frob_account_dialog_SOURCES = \
	frob-account-dialog.c \
	um-account-dialog.h \
	um-account-dialog.c \
	um-realm-manager.c \
	um-realm-manager.h \
	um-user.h \
	um-user.c \
	um-user-manager.c \
	um-user-manager.h \
	um-utils.h \
	um-utils.c \
	$(BUILT_SOURCES)

frob_account_dialog_LDADD = \
	$(libuser_accounts_la_LIBADD)

frob_account_dialog_CFLAGS = \
	$(AM_CFLAGS) \
	-DUIDIR=\""$(pkgdatadir)/ui/user-accounts"\"

polkitdir = $(datadir)/polkit-1/actions
polkit_in_files = org.gnome.controlcenter.user-accounts.policy.in

@INTLTOOL_POLICY_RULE@
polkit_DATA = $(polkit_in_files:.policy.in=.policy)

EXTRA_DIST =				\
	$(polkit_in_files)		\
	$(NULL)

CLEANFILES =				\
	$(BUILT_SOURCES)		\
	$(polkit_DATA)			\
	$(NULL)

MAINTAINERCLEANFILES =                  \
        *~                              \
        Makefile.in

-include $(top_srcdir)/git.mk
