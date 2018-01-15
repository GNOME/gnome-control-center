include $(top_srcdir)/Makefile.decl

SUBDIRS = appdata

AM_CPPFLAGS =					\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""\
	-DTEST_SRCDIR=\""$(srcdir)/"\"		\
	-DTEST_TOPSRCDIR=\""$(top_srcdir)/"\"	\
	-I$(top_srcdir)				\
	$(SHELL_CFLAGS)				\
	$(CHEESE_CFLAGS)			\
	$(WACOM_PANEL_CFLAGS)			\
	-I$(top_srcdir)/panels/common/		\
	-I$(top_srcdir)/subprojects/libgd

all-local: check-local

noinst_LTLIBRARIES = libshell.la

libshell_la_SOURCES = \
	cc-shell-model.c			\
	cc-shell-model.h

bin_PROGRAMS = gnome-control-center

common_sources =				\
	$(BUILT_SOURCES)			\
	main.c					\
	cc-application.c			\
	cc-application.h			\
	cc-shell-log.c				\
	cc-shell-log.h				\
	cc-shell-category-view.c		\
	cc-shell-category-view.h		\
	cc-shell-item-view.c			\
	cc-shell-item-view.h			\
	cc-editable-entry.c			\
	cc-editable-entry.h			\
	cc-panel-loader.c			\
	cc-panel-loader.h			\
	cc-panel.c				\
	cc-panel.h				\
	cc-shell.c				\
	cc-shell.h				\
	hostname-helper.c			\
	hostname-helper.h			\
	cc-hostname-entry.c			\
	cc-hostname-entry.h			\
	list-box-helper.h			\
	list-box-helper.c			\
	$(MARSHAL_FILES)

gnome_control_center_SOURCES =			\
	$(common_sources)			\
	cc-panel-list.c				\
	cc-panel-list.h				\
	cc-window.c				\
	cc-window.h

gnome_control_center_LDFLAGS = -export-dynamic

gnome_control_center_LDADD =						\
	libshell.la							\
	$(SHELL_LIBS)							\
	$(CHEESE_LIBS)							\
	$(top_builddir)/subprojects/libgd/libgd.la			\
	$(top_builddir)/panels/common/liblanguage.la			\
	$(top_builddir)/panels/common/libdevice.la			\
	$(top_builddir)/panels/background/libbackground.la		\
	$(top_builddir)/panels/color/libcolor.la			\
	$(top_builddir)/panels/datetime/libdate_time.la			\
	$(top_builddir)/panels/display/libdisplay.la			\
	$(top_builddir)/panels/info/libinfo.la				\
	$(top_builddir)/panels/keyboard/libkeyboard.la			\
	$(top_builddir)/panels/mouse/libmouse-properties.la		\
	$(top_builddir)/panels/notifications/libnotifications.la	\
	$(top_builddir)/panels/online-accounts/libonline-accounts.la	\
	$(top_builddir)/panels/power/libpower.la			\
	$(top_builddir)/panels/privacy/libprivacy.la			\
	$(top_builddir)/panels/region/libregion.la			\
	$(top_builddir)/panels/search/libsearch.la			\
	$(top_builddir)/panels/sharing/libsharing.la			\
	$(top_builddir)/panels/sound/libsound.la			\
	$(top_builddir)/panels/universal-access/libuniversal-access.la	\
	$(top_builddir)/panels/user-accounts/libuser-accounts.la	\
	$(top_builddir)/panels/printers/libprinters.la

if BUILD_WACOM
gnome_control_center_LDADD += $(top_builddir)/panels/wacom/libwacom-properties.la
endif

if BUILD_NETWORK
gnome_control_center_LDADD += $(top_builddir)/panels/network/libnetwork.la
endif

if BUILD_BLUETOOTH
gnome_control_center_LDADD += $(top_builddir)/panels/bluetooth/libbluetooth.la
endif

# Because it is confusing and somewhat problematic to directly add and compile
# cc-panel-loader.o by another directory (i.e. the shell search provider), we
# have to create a library and link it there, just like libshell.la.
noinst_LTLIBRARIES += libpanel_loader.la

libpanel_loader_la_SOURCES = \
	cc-panel-loader.c			\
	cc-panel-loader.h

libpanel_loader_la_CPPFLAGS = \
	$(AM_CPPFLAGS)				\
	-DCC_PANEL_LOADER_NO_GTYPES


# Dbus service file
servicefiledir = $(datadir)/dbus-1/services
servicefile_in_files = org.gnome.ControlCenter.service.in
servicefile_DATA = $(servicefile_in_files:.service.in=.service)
$(servicefile_DATA): $(servicefile_in_files) Makefile
	$(AM_V_GEN) sed -e 's|[@]bindir[@]|$(bindir)|' $< > $@

sysdir = $(datadir)/applications
sys_in_files = gnome-control-center.desktop.in
sys_DATA = $(sys_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

completiondir = $(datadir)/bash-completion/completions
completion_in_files = completions/gnome-control-center.in
completion_DATA = $(completion_in_files:.in=)
completions/gnome-control-center: completions/gnome-control-center.in list-panel.sh
	$(AM_V_GEN) mkdir -p `dirname $@` && cat $< | sed "s,@PANELS@,`$(srcdir)/list-panel.sh $(top_srcdir)`," > $@

resource_files = $(shell glib-compile-resources --generate-dependencies --sourcedir=$(srcdir) gnome-control-center.gresource.xml)
resources.c: gnome-control-center.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source $<

BUILT_SOURCES = resources.c

EXTRA_DIST =					\
	gnome-control-center.desktop.in.in	\
	$(servicefile_in_files)                 \
	$(completion_in_files)			\
	gnome-control-center.gresource.xml	\
	help-overlay.ui				\
	panel-list.ui				\
	window.ui				\
	$(resource_files)			\
	list-panel.sh

CLEANFILES = $(BUILT_SOURCES) $(completion_DATA) $(servicefile_DATA)
DISTCLEANFILES = gnome-control-center.desktop gnome-control-center.desktop.in

TEST_PROGS += test-hostname
noinst_PROGRAMS = test-hostname
test_hostname_SOURCES = hostname-helper.c hostname-helper.h test-hostname.c
test_hostname_LDADD = $(PANEL_LIBS) $(INFO_PANEL_LIBS)

EXTRA_DIST += hostnames-test.txt ssids-test.txt

-include $(top_srcdir)/git.mk
