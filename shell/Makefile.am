INCLUDES =					\
	-I$(top_srcdir)				\
	$(SHELL_CFLAGS)				\
	$(CHEESE_CFLAGS)

bin_PROGRAMS = gnome-control-center

gnome_control_center_SOURCES =			\
	control-center.c			\
	cc-shell-log.c				\
	cc-shell-log.h				\
	gnome-control-center.c			\
	gnome-control-center.h			\
	cc-shell-category-view.c		\
	cc-shell-category-view.h		\
	cc-shell-item-view.c			\
	cc-shell-item-view.h			\
	cc-shell-model.c			\
	cc-shell-model.h			\
	cc-editable-entry.c			\
	cc-editable-entry.h			\
	cc-panel-loader.c			\
	cc-panel-loader.h			\
	cc-panel.c				\
	cc-panel.h				\
	cc-shell.c				\
	cc-shell.h				\
	$(MARSHAL_FILES)

gnome_control_center_LDFLAGS = -export-dynamic

gnome_control_center_LDADD =						\
	$(SHELL_LIBS)							\
	$(CHEESE_LIBS)							\
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
	$(top_builddir)/panels/screen/libscreen.la			\
	$(top_builddir)/panels/search/libsearch.la			\
	$(top_builddir)/panels/sound/libsound.la			\
	$(top_builddir)/panels/universal-access/libuniversal-access.la	\
	$(top_builddir)/panels/user-accounts/libuser-accounts.la

if BUILD_WACOM
gnome_control_center_LDADD += $(top_builddir)/panels/wacom/libwacom-properties.la
endif

if BUILD_PRINTERS
gnome_control_center_LDADD += $(top_builddir)/panels/printers/libprinters.la
endif

if BUILD_NETWORK
gnome_control_center_LDADD += $(top_builddir)/panels/network/libnetwork.la
endif

if BUILD_BLUETOOTH
gnome_control_center_LDADD += $(top_builddir)/panels/bluetooth/libbluetooth.la
endif

AM_CPPFLAGS =							\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""		\
	-DUIDIR="\"$(uidir)\""

uidir = $(pkgdatadir)/ui
ui_DATA = shell.ui

sysdir = $(datadir)/applications
sys_in_files = gnome-control-center.desktop.in
sys_DATA = $(sys_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

completiondir = $(datadir)/bash-completion/completions
completion_in_files = gnome_control_center_completion.bash.in
completion_DATA = $(completion_in_files:.bash.in=.bash)
gnome_control_center_completion.bash: gnome_control_center_completion.bash.in list-panel.sh
	$(AM_V_GEN) cat $< | sed "s,@PANELS@,`$(srcdir)/list-panel.sh $(top_srcdir)`," > $@

EXTRA_DIST =					\
	$(ui_DATA)				\
	gnome-control-center.desktop.in.in	\
	$(completion_in_files)			\
	list-panel.sh

CLEANFILES = $(completion_DATA)
DISTCLEANFILES = gnome-control-center.desktop gnome-control-center.desktop.in

-include $(top_srcdir)/git.mk
