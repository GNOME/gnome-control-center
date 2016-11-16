cappletname = info

AM_CPPFLAGS =							\
	$(PANEL_CFLAGS)						\
	$(INFO_PANEL_CFLAGS)					\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""		\
	-DDATADIR="\"$(datadir)\""				\
	-DGNOME_SESSION_DIR="\"$(gnome_session_libexecdir)\""	\
	-DBINDIR="\"$(bindir)\""				\
	$(NULL)

noinst_LTLIBRARIES = libinfo.la

BUILT_SOURCES =			\
	cc-info-resources.c	\
	cc-info-resources.h

libinfo_la_SOURCES =		\
	$(BUILT_SOURCES)	\
	cc-info-panel.c		\
	cc-info-panel.h		\
	gsd-disk-space-helper.h	\
	gsd-disk-space-helper.c	\
	info-cleanup.h		\
	info-cleanup.c

libinfo_la_LIBADD = $(PANEL_LIBS) $(INFO_PANEL_LIBS)

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/info.gresource.xml)
cc-info-resources.c: info.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_info $<
cc-info-resources.h: info.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_info $<

@INTLTOOL_DESKTOP_RULE@

desktopdir = $(datadir)/applications
desktop_in_files = gnome-info-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

SPACEDIR=$(top_srcdir)/../gnome-settings-daemon/plugins/housekeeping/
SPACEFILES=gsd-disk-space-helper.c gsd-disk-space-helper.h
update-from-gsd:
	FILES="$(SPACEFILES)" DIR="$(SPACEDIR)" $(top_srcdir)/update-from-gsd.sh && changed=true ; \
	git commit -m "info: Update from gnome-settings-daemon" $(SPACEFILES)

CLEANFILES = $(desktop_in_files) $(desktop_DATA) $(BUILT_SOURCES)
EXTRA_DIST = $(resource_files) info.gresource.xml

-include $(top_srcdir)/git.mk
