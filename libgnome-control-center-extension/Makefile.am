NULL =

INCLUDES =						\
	$(EXTENSION_COMMON_CFLAGS)			\
	$(NULL)

lib_LTLIBRARIES = libgnome-control-center-extension.la

libgnome_control_center_extension_include_HEADERS =	\
	cc-page.h					\
	cc-panel.h					\
	$(NULL)

libgnome_control_center_extension_la_SOURCES =		\
	cc-page.h					\
	cc-page.c					\
	cc-panel.h					\
	cc-panel.c					\
	$(NULL)

#        -version-info @GNOME_CONTROL_CENTER_EXTENSION_VERSION_INFO@
libgnome_control_center_extension_la_LDFLAGS =		\
        -no-undefined					\
        $(NULL)

libgnome_control_center_extension_la_LIBADD = 		\
	$(top_builddir)/capplets/common/libcommon.la	\
	$(EXTENSION_COMMON_CFLAGS)			\
	$(NULL)

libgnome_control_center_extension_la_LIBTOOLFLAGS = --tag=disable-static

libgnome_control_center_extension_includedir =		\
	$(includedir)/gnome-control-center-1/libgnome-control-center-extension	\
	$(NULL)

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libgnome-control-center-extension.pc

EXTRA_DIST =					\
        libgnome-control-center-extension.pc.in	\
        $(NULL)

AM_CPPFLAGS =							\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""		\
	-DEXTENSION_DIR="\"$(libdir)/control-center-1/extensions\""					\
	-DUIDIR="\"$(uidir)\""					\
	-DMENUDIR="\"$(menudir)\""

-include $(top_srcdir)/git.mk
