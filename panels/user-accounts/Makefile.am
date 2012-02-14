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
	-I$(srcdir)/../common/				\
	-I$(srcdir)/../../shell/			\
	$(PANEL_CFLAGS)					\
	$(USER_ACCOUNTS_PANEL_CFLAGS)

if BUILD_CHEESE
AM_CPPFLAGS += $(CHEESE_CFLAGS)
endif

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
	um-user-module.c

libuser_accounts_la_LIBADD = 		\
	$(PANEL_LIBS)			\
	$(USER_ACCOUNTS_PANEL_LIBS)	\
	$(top_builddir)/panels/common/liblanguage.la \
	-lcrypt				\
	-lm

if BUILD_CHEESE
libuser_accounts_la_LIBADD += $(CHEESE_LIBS)
endif

libuser_accounts_la_LDFLAGS = $(PANEL_LDFLAGS)

CLEANFILES =				\
	$(BUILT_SOURCES)		\
	$(NULL)

MAINTAINERCLEANFILES =                  \
        *~                              \
        Makefile.in

-include $(top_srcdir)/git.mk
