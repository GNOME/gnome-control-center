pixmapdir = $(datadir)/pixmaps
pixmap_DATA = xst-keys.png

INCLUDES = \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"		\
	@RMHELPER_CFLAGS@					\
	-DUH_PATH=\""$(sbindir)/root-manager"\"			\
	-DUH_KEY_PIXMAP_PATH=\""$(pixmapdir)/xst-keys.png"\"	\
	-DDEBUG_USERHELPER

bin_PROGRAMS = root-manager-helper

sbin_PROGRAMS = root-manager

root_manager_SOURCES = \
	root-manager.c	\
	root-manager.h \
	shvar.c \
	shvar.h

root_manager_LDADD = -lpam -lpam_misc -ldl

root_manager_LDFLAGS = $(shell glib-config --libs)

root_manager_helper_SOURCES = \
	root-manager-helper.c	\
	root-manager-wrap.c	\
	root-manager-wrap.h

root_manager_helper_LDFLAGS = @RMHELPER_LIBS@

EXTRA_DIST = xst-keys.png
