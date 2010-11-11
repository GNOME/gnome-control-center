# This is used in GNOMECC_CAPPLETS_CFLAGS
cappletname = default-applications

ccpanelsdir = $(PANELS_DIR)
ccpanels_LTLIBRARIES = libdefault-applications.la

libdefault_applications_la_SOURCES = \
	default-applications-module.c \
	cc-default-applications-panel.c cc-default-applications-panel.h \
	gnome-da-capplet.c gnome-da-capplet.h \
	gnome-da-xml.c gnome-da-xml.h \
	gnome-da-item.c gnome-da-item.h

libdefault_applications_la_LIBADD = \
	$(PANEL_LIBS) \
	$(XML_LIBS) \
	$(GNOMECC_CAPPLETS_LIBS)

libdefault_applications_la_LDFLAGS = $(PANEL_LDFLAGS)

bin_SCRIPTS = gnome-at-visual gnome-at-mobility

@INTLTOOL_DESKTOP_RULE@

uidir = $(pkgdatadir)/ui
dist_ui_DATA = gnome-default-applications-properties.ui

gnome-at-visual: gnome-at-commandline.in
	cp $< $@

gnome-at-mobility: gnome-at-commandline.in
	cp $< $@

desktopdir = $(datadir)/applications
Desktop_in_files = gnome-default-applications-panel.desktop.in
desktop_DATA = $(Desktop_in_files:.desktop.in=.desktop)

pkgconfigdir = $(datadir)/pkgconfig
pkgconfig_DATA = gnome-default-applications.pc

autostartdir = $(sysconfdir)/xdg/autostart
autostart_in_files = gnome-at-session.desktop.in
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

xmldata_in_files = gnome-default-applications.xml.in
xmldatadir = $(pkgdatadir)/default-apps
xmldata_DATA = $(xmldata_in_files:.xml.in=.xml)
@INTLTOOL_XML_RULE@

INCLUDES   = 					\
	$(GNOMECC_CAPPLETS_CFLAGS)		\
	$(XML_CFLAGS)				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"\
	-DGNOMECC_UI_DIR=\""$(uidir)"\"		\
	-DGNOMECC_APPS_DIR=\""$(xmldatadir)"\"

icons16dir = $(datadir)/icons/hicolor/16x16/apps
dist_icons16_DATA = icons/16x16/gnome-settings-default-applications.png
icons22dir = $(datadir)/icons/hicolor/22x22/apps
dist_icons22_DATA = icons/22x22/gnome-settings-default-applications.png
icons24dir = $(datadir)/icons/hicolor/24x24/apps
dist_icons24_DATA = icons/24x24/gnome-settings-default-applications.png
icons32dir = $(datadir)/icons/hicolor/32x32/apps
dist_icons32_DATA = icons/32x32/gnome-settings-default-applications.png
icons48dir = $(datadir)/icons/hicolor/48x48/apps
dist_icons48_DATA = icons/48x48/gnome-settings-default-applications.png

gtk_update_icon_cache = gtk-update-icon-cache -f -t $(datadir)/icons/hicolor
uninstall-hook: update-icon-cache
install-data-hook: update-icon-cache
update-icon-cache:
	@-if test -z "$(DESTDIR)"; then \
		echo "Updating Gtk icon cache."; \
		$(gtk_update_icon_cache); \
	else \
		echo "*** Icon cache not updated.  After install, run this:"; \
		echo "***   $(gtk_update_icon_cache)"; \
	fi


CLEANFILES = $(GNOMECC_CAPPLETS_CLEANFILES) $(Desktop_in_files) $(desktop_DATA) $(xmldata_DATA) $(autostart_DATA) $(bin_SCRIPTS)
EXTRA_DIST = $(xmldata_in_files) gnome-default-applications.pc.in

-include $(top_srcdir)/git.mk
