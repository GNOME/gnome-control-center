INCLUDES = \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-I$(top_srcdir)/intl -I$(top_builddir)/intl	\
	-I$(includedir)  $(shell gtk-config --cflags)	\
	-DDEBUG_USERHELPER

bin_PROGRAMS = root-manager-helper

sbin_PROGRAMS = root-manager

root_manager_SOURCES = \
	root-manager.c	\
	root-manager.h

root_manager_LDADD = -lpam -lpam_misc -ldl

root_manager_LDFLAGS = $(shell glib-config --libs)

root_manager_helper_SOURCES = \
	root-manager-helper.c	\
	root-manager-wrap.c	\
	root-manager-wrap.h	\
	userdialogs.c		\
	userdialogs.h

root_manager_helper_LDFLAGS = $(shell gtk-config --libs)
