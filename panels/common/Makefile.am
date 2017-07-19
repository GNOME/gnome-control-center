# This is used in PANEL_CFLAGS
cappletname = common

noinst_LTLIBRARIES = liblanguage.la libdevice.la

AM_CPPFLAGS =						\
	$(DEVICES_CFLAGS)				\
	$(PANEL_CFLAGS)					\
	$(LIBLANGUAGE_CFLAGS)

BUILT_SOURCES =			\
	cc-common-resources.c	\
	cc-common-resources.h	\
	gsd-common-enums.c	\
	gsd-common-enums.h

liblanguage_la_SOURCES =		\
	$(BUILT_SOURCES)		\
	cc-util.c			\
	cc-util.h			\
	cc-common-language.c		\
	cc-common-language.h		\
	cc-language-chooser.c		\
	cc-language-chooser.h

liblanguage_la_LIBADD = 		\
	$(LIBLANGUAGE_LIBS)

#libdevice
GSD_COMMON_ENUM_FILES = gsd-common-enums.c gsd-common-enums.h

BUILT_SOURCES += $(GSD_COMMON_ENUM_FILES)

gsd-common-enums.h: gsd-device-manager.h Makefile
	$(AM_V_GEN)($(GLIB_MKENUMS) \
			--fhead "#ifndef GSD_COMMON_ENUMS_H\n#define GSD_COMMON_ENUMS_H\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define GSD_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
			--ftail "G_END_DECLS\n\n#endif /* !GSD_COMMON_ENUMS_H */" \
		$(srcdir)/gsd-device-manager.h > $@)

gsd-common-enums.c: gsd-device-manager.h Makefile gsd-common-enums.h
	$(AM_V_GEN)($(GLIB_MKENUMS) \
			--fhead "#include \"gsd-device-manager.h\"\n#include \"gsd-common-enums.h\"\n" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" \
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		$(srcdir)/gsd-device-manager.h > $@)

libdevice_la_SOURCES =			\
	gsd-common-enums.c 		\
	gsd-common-enums.h		\
	gsd-input-helper.c		\
	gsd-input-helper.h		\
	gsd-device-manager.c		\
	gsd-device-manager.h		\
	gsd-device-manager-x11.c	\
	gsd-device-manager-x11.h	\
	gnome-settings-bus.h

libdevice_la_LIBADD =			\
	$(DEVICES_LIBS)

if HAVE_WAYLAND
AM_CPPFLAGS += $(UDEV_CFLAGS)
libdevice_la_LIBADD += $(UDEV_LIBS)
libdevice_la_SOURCES +=			\
	gsd-device-manager-udev.c	\
	gsd-device-manager-udev.h
endif

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/common.gresource.xml)
cc-common-resources.c: common.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_common $<
cc-common-resources.h: common.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_common $<

rulesdir = $(datadir)/polkit-1/rules.d/
rules_DATA = gnome-control-center.rules

EXTRA_DIST = $(rules_DATA) $(resource_files) common.gresource.xml
CLEANFILES = $(BUILT_SOURCES) $(GSD_COMMON_ENUM_FILES)

INPUTDIR=$(top_srcdir)/../gnome-settings-daemon/plugins/common/
INPUTFILES=				\
	gsd-input-helper.c 		\
	gsd-input-helper.h 		\
	gsd-device-manager.c 		\
	gsd-device-manager.h		\
	gsd-device-manager-x11.c	\
	gsd-device-manager-x11.h	\
	gsd-device-manager-udev.c	\
	gsd-device-manager-udev.h

update-from-gsd:
	FILES="$(INPUTFILES)" DIR="$(INPUTDIR)" $(top_srcdir)/update-from-gsd.sh && changed=true ; \
	git commit -m "common: Update from gnome-settings-daemon" $(INPUTFILES)

-include $(top_srcdir)/git.mk
