panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

i18n.merge_file(
  type: 'desktop',
  input: desktop + '.in',
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: control_center_desktopdir
)

polkit = 'org.gnome.controlcenter.@0@.policy'.format(cappletname)

i18n.merge_file(
  input: polkit + '.in',
  output: polkit,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(control_center_datadir, 'polkit-1', 'actions')
)

resources = gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  export: true
)

sources = files(
  'cc-datetime-panel.c',
  'cc-tz-item.c',
  'cc-tz-dialog.c',
  'date-endian.c',
  'tz.c'
)

enums = 'gdesktop-enums-types'
enums_header = files(
  gsettings_desktop_dep.get_variable(pkgconfig: 'prefix') + '/include/gsettings-desktop-schemas/gdesktop-enums.h',
  'cc-datetime-panel.h'
)

sources += gnome.mkenums(
  enums + '.h',
  sources: enums_header,
  fhead: '#pragma once\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n',
  fprod: '/* enumerations from "@filename@" */\n',
  vhead: 'GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define G_DESKTOP_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n',
  ftail: 'G_END_DECLS\n'
)

sources += gnome.mkenums(
  enums + '.c',
  sources: enums_header,
  fhead: '#include <gdesktop-enums.h>\n#include "gdesktop-enums-types.h"\n#include "cc-datetime-panel.h"',
  fprod: '\n/* enumerations from "@filename@" */',
  vhead: 'GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {',
  vprod: '      { @VALUENAME@, "@VALUENAME@", "@valuenick@" },',
  vtail: '      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static ("@EnumName@", values);\n  }\n  return etype;\n}\n'
)

gdbus = 'timedated'
gdbus_iface_xml = gdbus + '1-interface.xml'

# This requires running d-bus session and accessible timedate1 daemon
# FIXME: need to find a way how to filter out unnecessary d-bus stuff (introspectable, properties)
#timedated1-interface.xml:
# gdbus introspect          \
#   --xml           \
#   --system          \
#   --dest org.freedesktop.timedate1    \
#   --object-path /org/freedesktop/timedate1  \
#    > timedated1-interface.xml
'''
gdbus_iface = custom_target(
  gdbus_iface_xml,
  output: gdbus_iface_xml,
  capture: true,
  command: [
    find_program('gdbus'),
    'introspect',
    '--xml',
    '--system',
    '--dest', 'org.freedesktop.timedate1',
    '--object-path', '/org/freedesktop/timedate1'
  ],
)
'''

sources += gnome.gdbus_codegen(
  gdbus,
  gdbus_iface_xml,
  interface_prefix: 'org.freedesktop.'
)

deps = common_deps + [
  gdk_pixbuf_dep,
  gnome_desktop_dep,
  polkit_gobject_dep
]

cflags += [
  '-DGNOMECC_DATA_DIR="@0@"'.format(control_center_pkgdatadir)
]

datetime_panel_lib = static_library(
  cappletname,
  sources: sources + resources,
  include_directories: [ top_inc, common_inc ],
  dependencies: deps,
  c_args: cflags
)
panels_libs += datetime_panel_lib

datetime_panel_lib_dep = declare_dependency(
              sources : resources,
  include_directories : [top_inc, include_directories('.')],
            link_with : datetime_panel_lib
)

subdir('po-timezones')
subdir('icons')
