# This is used in PANEL_CFLAGS
cappletname = background

slideshowicondir = $(datadir)/icons/hicolor/scalable/categories/
dist_slideshowicon_DATA = slideshow-symbolic.svg

slideshowemblemdir = $(datadir)/icons/hicolor/scalable/emblems/
dist_slideshowemblem_DATA = slideshow-emblem.svg

AM_CPPFLAGS =						\
	$(PANEL_CFLAGS)					\
	$(BACKGROUND_PANEL_CFLAGS)			\
	-DGNOMELOCALEDIR="\"$(datadir)/locale\""	\
	-DDATADIR="\"$(datadir)\""			\
	-DGNOME_DESKTOP_USE_UNSTABLE_API		\
	$(NULL)

BUILT_SOURCES = 			\
	cc-background-resources.c 	\
	cc-background-resources.h 	\
	gdesktop-enums-types.c		\
	gdesktop-enums-types.h

noinst_LTLIBRARIES = libbackground.la libbackground-chooser.la

libbackground_chooser_la_SOURCES =	\
	$(BUILT_SOURCES)		\
	cc-background-chooser-dialog.c	\
	cc-background-chooser-dialog.h	\
	cc-background-grilo-miner.c	\
	cc-background-grilo-miner.h	\
	cc-background-item.c		\
	cc-background-item.h		\
	cc-background-xml.c		\
	cc-background-xml.h		\
	bg-source.c			\
	bg-source.h			\
	bg-pictures-source.c		\
	bg-pictures-source.h		\
	bg-wallpapers-source.c		\
	bg-wallpapers-source.h		\
	bg-colors-source.c		\
	bg-colors-source.h

libbackground_chooser_la_LIBADD = $(PANEL_LIBS) $(BACKGROUND_PANEL_LIBS)

libbackground_la_SOURCES =		\
	cc-background-panel.c		\
	cc-background-panel.h

libbackground_la_LIBADD = $(PANEL_LIBS) $(BACKGROUND_PANEL_LIBS) libbackground-chooser.la

noinst_PROGRAMS = test-chooser-dialog
test_chooser_dialog_SOURCES = test-chooser-dialog.c
test_chooser_dialog_LDADD = libbackground-chooser.la $(PANEL_LIBS) $(BACKGROUND_PANEL_LIBS)

resource_files = $(shell glib-compile-resources --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/background.gresource.xml)
cc-background-resources.c: background.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-source --c-name cc_background $<
cc-background-resources.h: background.gresource.xml $(resource_files)
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(srcdir) --generate-header --c-name cc_background $<

gdesktop-enums-types.h: stamp-gdesktop-enums-types.h
	@true

stamp-gdesktop-enums-types.h: $(GDESKTOP_PREFIX)/include/gsettings-desktop-schemas/gdesktop-enums.h cc-background-item.h Makefile
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#ifndef __GDESKTOP_ENUMS_TYPES_H__\n#define __GDESKTOP_ENUMS_TYPES_H__\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n" \
			--fprod "/* enumerations from \"@filename@\" */\n" \
			--vhead "GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define G_DESKTOP_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
			--ftail "G_END_DECLS\n\n#endif /* __GDESKTOP_ENUMS_TYPES_H__ */" $(GDESKTOP_PREFIX)/include/gsettings-desktop-schemas/gdesktop-enums.h cc-background-item.h) >> xgen-gtbh \
	&& (cmp -s xgen-gtbh gdesktop-enums-types.h || cp xgen-gtbh gdesktop-enums-types.h ) \
	&& rm -f xgen-gtbh \
	&& echo timestamp > $(@F)

gdesktop-enums-types.c: $(GDESKTOP_PREFIX)/include/gsettings-desktop-schemas/gdesktop-enums.h cc-background-item.h Makefile gdesktop-enums-types.h
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) \
			--fhead "#include <gdesktop-enums.h>\n#include \"gdesktop-enums-types.h\"\n#include \"cc-background-item.h\"" \
			--fprod "\n/* enumerations from \"@filename@\" */" \
			--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" \
			--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
			--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
		$(GDESKTOP_PREFIX)/include/gsettings-desktop-schemas/gdesktop-enums.h cc-background-item.h) > xgen-gtbc \
	&& cp xgen-gtbc gdesktop-enums-types.c  \
	&& rm -f xgen-gtbc

@INTLTOOL_DESKTOP_RULE@

pixmapdir  = $(pkgdatadir)/pixmaps
dist_pixmap_DATA = \
	noise-texture-light.png

desktopdir = $(datadir)/applications
desktop_in_files = gnome-background-panel.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

CLEANFILES = $(desktop_in_files) $(desktop_DATA) $(BUILT_SOURCES) stamp-gdesktop-enums-types.h
EXTRA_DIST = $(resource_files) background.gresource.xml meson.build

-include $(top_srcdir)/git.mk
