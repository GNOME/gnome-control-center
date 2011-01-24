SUBDIRS = data

# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = user-accounts
NULL =

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libuser-accounts.la

AM_CPPFLAGS =						\
	$(GNOMECC_CAPPLETS_CFLAGS)			\
	-DDATADIR=\""$(datadir)"\"			\
	-DUIDIR=\""$(pkgdatadir)/ui/user-accounts"\"	\
	-DLIBLOCALEDIR=\""$(prefix)/lib/locale"\"       \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"        \
	-DUM_PIXMAP_DIR=\""$(pkgdatadir)/pixmaps"\"	\
	-I$(srcdir)/../common/				\
	$(PANEL_CFLAGS)					\
	$(GNOME_DESKTOP_CFLAGS) 			\
	$(POLKIT_CFLAGS)				\
	$(CHEESE_CFLAGS)				\
	$(DBUS_CFLAGS)					\
	$(GIO_CFLAGS)					\
	$(WARN_CFLAGS)

MARSHALFILES = marshal.c marshal.h
BUILT_SOURCES = $(MARSHALFILES)

marshal.h: fprintd-marshal.list
	@GLIB_GENMARSHAL@ --prefix=fprintd_marshal $< --header > $@
marshal.c: fprintd-marshal.list
	@GLIB_GENMARSHAL@ --prefix=fprintd_marshal $< --body --header > $@

libuser_accounts_la_CFLAGS = 		\
	$(PANEL_CFLAGS)

libuser_accounts_la_SOURCES =		\
	um-account-type.h		\
	um-account-type.c 		\
	um-user.h 			\
	um-user.c 			\
	um-user-manager.h 		\
	um-user-manager.c 		\
	um-account-dialog.h		\
	um-account-dialog.c		\
	um-language-dialog.h 		\
	um-language-dialog.c		\
	um-lockbutton.h			\
	um-lockbutton.c			\
	um-login-options.h		\
	um-login-options.c		\
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
	um-strength-bar.h		\
	um-strength-bar.c		\
	run-passwd.h			\
	run-passwd.c			\
	$(MARSHALFILES)			\
	um-editable-button.h		\
	um-editable-button.c		\
	um-editable-entry.h		\
	um-editable-entry.c		\
	um-editable-combo.h		\
	um-editable-combo.c		\
	um-user-panel.h 		\
	um-user-panel.c			\
	um-user-module.c

libuser_accounts_la_LIBADD = 		\
	$(PANEL_LIBS)			\
	$(GNOME_DESKTOP_LIBS)		\
	$(POLKIT_LIBS)			\
	$(CHEESE_LIBS)			\
	$(DBUS_LIBS)			\
	$(GIO_LIBS)			\
	$(CAPPLET_LIBS)			\
	$(top_builddir)/panels/common/liblanguage.la \
	-lcrypt

libuser_accounts_la_LDFLAGS = $(PANEL_LDFLAGS)

EXTRA_DIST = \
	fprintd-marshal.list

CLEANFILES =				\
	$(BUILT_SOURCES)		\
	$(NULL)

MAINTAINERCLEANFILES =                  \
        *~                              \
        Makefile.in

-include $(top_srcdir)/git.mk
