cappletname = online-accounts

SUBDIRS = icons

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(ONLINE_ACCOUNTS_PANEL_CFLAGS)				\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	$(NULL)

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libonline-accounts.la

libonline_accounts_la_SOURCES =		\
	cc-online-accounts-add-account-dialog.c	\
	cc-online-accounts-add-account-dialog.h	\
	cc-online-accounts-model.c	\
	cc-online-accounts-model.h	\
	cc-online-accounts-panel.c	\
	cc-online-accounts-panel.h

libonline_accounts_la_LIBADD = $(PANEL_LIBS) $(ONLINE_ACCOUNTS_PANEL_LIBS)
libonline_accounts_la_LDFLAGS = $(PANEL_LDFLAGS)

uidir = $(pkgdatadir)/ui
dist_ui_DATA = online-accounts.ui

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-online-accounts-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk
