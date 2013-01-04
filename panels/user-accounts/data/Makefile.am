SUBDIRS = faces

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
Desktop_in_files = gnome-user-accounts-panel.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

EXTRA_DIST = 					\
	gnome-user-accounts-panel.desktop.in.in	\
	org.freedesktop.realmd.xml

CLEANFILES =					\
	gnome-user-accounts-panel.desktop	\
	$(NULL)

DISTCLEANFILES =				\
	$(NULL)

-include $(top_srcdir)/git.mk
