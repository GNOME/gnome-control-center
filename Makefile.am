SUBDIRS = archiver capplets root-manager control-center po intl

dist-hook: control-center.spec
	cp control-center.spec $(distdir)

ccenterdir      = $(datadir)/control-center/capplets
paneldir        = $(datadir)/gnome/apps/Settings

Directory_files = \
	Gnome.directory

install-data-local:
	$(INSTALL_DATA) $(srcdir)/Gnome.directory $(DESTDIR)$(ccenterdir)/.directory

EXTRA_DIST = \
	control-center.spec.in			\
	$(Directory_files)
