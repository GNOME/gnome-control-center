cappletname = notifications

INCLUDES = 						\
	$(PANEL_CFLAGS)					\
	$(NOTIFICATIONS_PANEL_CFLAGS)			\
	-DGNOMECC_UI_DIR="\"$(uidir)\""			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DGNOMECC_DATA_DIR="\"$(pkgdatadir)\""		\
	$(NULL)

noinst_LTLIBRARIES = libnotifications.la

libnotifications_la_SOURCES =		\
	cc-edit-dialog.c		\
	cc-edit-dialog.h		\
	cc-notifications-panel.c	\
	cc-notifications-panel.h

libnotifications_la_LIBADD = $(NOTIFICATIONS_PANEL_LIBS) $(PANEL_LIBS)

uidir = $(pkgdatadir)/ui
dist_ui_DATA = notifications.ui

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-notifications-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA)

-include $(top_srcdir)/git.mk
