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

EXTRA_DIST = \
	control-center.spec.in	\
	xml-i18n-extract.in \
	xml-i18n-merge.in \
	xml-i18n-update.in

