panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

desktop_in = configure_file(
  input: desktop + '.in.in',
  output: desktop + '.in',
  configuration: desktop_conf
)

i18n.merge_file(
  desktop,
  type: 'desktop',
  input: desktop_in,
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: control_center_desktopdir
)

install_data(
  'slideshow-symbolic.svg',
  install_dir: join_paths(control_center_icondir, 'hicolor', 'scalable', 'categories')
)

install_data(
  'slideshow-emblem.svg',
  install_dir: join_paths(control_center_icondir, 'hicolor', 'scalable', 'emblems')
)

install_data(
  'noise-texture-light.png',
  install_dir: join_paths(control_center_pkgdatadir, 'pixmaps')
)

common_sources = []

enums = 'gdesktop-enums-types'
enums_header = files(
  gsettings_desktop_dep.get_pkgconfig_variable('prefix') + '/include/gsettings-desktop-schemas/gdesktop-enums.h',
  'cc-background-item.h'
)

common_sources += gnome.mkenums(
  enums + '.h',
  sources: enums_header,
  fhead: '#pragma once\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n',
  fprod: '/* enumerations from "@filename@" */\n',
  vhead: 'GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define G_DESKTOP_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n',
  ftail: 'G_END_DECLS\n'
)

common_sources += gnome.mkenums(
  enums + '.c',
  sources: enums_header,
  fhead: '#include <gdesktop-enums.h>\n#include "gdesktop-enums-types.h"\n#include "cc-background-item.h"',
  fprod: '\n/* enumerations from "@filename@" */',
  vhead: 'GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {',
  vprod: '      { @VALUENAME@, "@VALUENAME@", "@valuenick@" },',
  vtail: '      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static ("@EnumName@", values);\n  }\n  return etype;\n}\n'
)

resource_data = files(
  'cc-background-chooser.ui',
  'cc-background-panel.ui',
  'cc-background-preview.ui',
  'preview.css',
)

common_sources += gnome.compile_resources(
  'cc-@0@-resources'.format(cappletname),
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  dependencies: resource_data,
  export: true
)

sources = common_sources + files(
  'bg-colors-source.c',
  'bg-pictures-source.c',
  'bg-recent-source.c',
  'bg-source.c',
  'bg-wallpapers-source.c',
  'cc-background-chooser.c',
  'cc-background-grilo-miner.c',
  'cc-background-item.c',
  'cc-background-panel.c',
  'cc-background-preview.c',
  'cc-background-xml.c',
)

deps = common_deps + [
  gdk_pixbuf_dep,
  gnome_desktop_dep,
  goa_dep,
  libxml_dep,
  dependency('cairo-gobject'),
  dependency('grilo-0.3', version: '>= 0.3.0')
]

cflags += [
  '-DDATADIR="@0@"'.format(control_center_datadir),
  '-DGNOME_DESKTOP_USE_UNSTABLE_API'
]

if gnome_desktop_dep.version().version_compare('>=3.35.4')
  cflags += '-DGNOME_DESKTOP_BG_API_BREAK'
endif

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags,
)
