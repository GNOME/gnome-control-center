SUBDIRS = archiver capplets control-center po intl
#I removed the root-manager for now
#
# it generated this error :
# root-manager.c:55:31: security/pam_appl.h: No such file or directory
# while compiling. I guess we need to check for pam or something
# in the configure.in file. Since we are not using it now, i will
# remove it from the default build. Chema

dist-hook: control-center.spec
	cp control-center.spec $(distdir)

ccenterdir      = $(datadir)/control-center/capplets
paneldir        = $(datadir)/gnome/apps/Settings

directory_in_files = \
	Gnome.directory.in
noinst_DATA = $(directory_in_files:.directory.in=.directory)
@XML_I18N_MERGE_DIRECTORY_RULE@

install-data-local:
	$(INSTALL_DATA) $(srcdir)/Gnome.directory $(DESTDIR)$(ccenterdir)/.directory

EXTRA_DIST = \
	control-center.spec.in	\
	$(directory_in_files) \
	$(noinst_DATA) \
	xml-i18n-extract.in \
	xml-i18n-merge.in \
	xml-i18n-update.in
