common_inc = include_directories('.')

common_sources = []

enums = 'gsd-common-enums'
enums_header = files('gsd-device-manager.h')

common_sources += gnome.mkenums(
  enums + '.h',
  sources: enums_header,
  fhead: '#ifndef GSD_COMMON_ENUMS_H\n#define GSD_COMMON_ENUMS_H\n\n#include <glib-object.h>\n\nG_BEGIN_DECLS\n',
  fprod: '/* enumerations from "@filename@" */\n',
  vhead: 'GType @enum_name@_get_type (void) G_GNUC_CONST;\n#define GSD_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n',
  ftail: 'G_END_DECLS\n\n#endif /* !GSD_COMMON_ENUMS_H */'
)

common_sources += gnome.mkenums(
  enums + '.c',
  sources: enums_header,
  fhead: '#include "gsd-device-manager.h"\n#include "gsd-common-enums.h"\n',
  fprod: '\n/* enumerations from "@filename@" */',
  vhead: 'GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {',
  vprod: '      { @VALUENAME@, "@VALUENAME@", "@valuenick@" },',
  vtail: '      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static ("@EnumName@", values);\n  }\n  return etype;\n}\n'
)

sources = files(
  'cc-hostname-entry.c',
  'hostname-helper.c',
  'list-box-helper.c',
)

libwidgets = static_library(
  'widgets',
  sources: sources,
  include_directories: top_inc,
  dependencies: common_deps + [ polkit_gobject_dep ]
)
libwidgets_dep = declare_dependency(
  include_directories: common_inc,
  link_with: libwidgets
)

sources = common_sources + files(
  'cc-common-language.c',
  'cc-language-chooser.c',
  'cc-util.c'
)

resource_data = files('language-chooser.ui')

sources += gnome.compile_resources(
  'cc-common-resources',
  'common.gresource.xml',
  c_name: 'cc_common',
  dependencies: resource_data,
  export: true
)

deps = common_deps + [
  gnome_desktop_dep,
  dependency('fontconfig')
]

liblanguage = static_library(
  'language',
  sources: sources,
  include_directories: top_inc,
  dependencies: deps
)

liblanguage_dep = declare_dependency(
  include_directories: common_inc,
  link_with: liblanguage
)

gsd_headers = [
  'gsd-device-manager.h',
  'gsd-device-manager-udev.h',
  'gsd-device-manager-x11.h',
  'gsd-input-helper.h'
]

gsd_sources_udev = ['gsd-device-manager-udev.c']

gsd_sources = [
  'gsd-device-manager.c',
  'gsd-device-manager-x11.c',
  'gsd-input-helper.c'
]

sources = common_sources + files(gsd_sources)

deps = common_deps

if enable_wayland
  sources += files(gsd_sources_udev)

  deps += wayland_deps
endif

libdevice = static_library(
  'device',
  sources: sources,
  include_directories: top_inc,
  dependencies: deps
)

libdevice_dep = declare_dependency(
  include_directories: common_inc,
  link_with: libdevice
)

install_data(
  'gnome-control-center.rules',
  install_dir: join_paths(control_center_datadir, 'polkit-1', 'rules.d')
)

# FIXME: workaround for updating different sources code
input_dir = join_paths(meson.source_root(), '..', 'gnome-settings-daemon', 'plugins', 'common')

script_conf = configuration_data()
script_conf.set('program', update_from_gsd.path())
script_conf.set('input_dir', input_dir)
script_conf.set('working_dir', meson.current_source_dir())
script_conf.set('source_files', ' '.join(gsd_sources + gsd_sources_udev + gsd_headers))
script_conf.set('source_message', 'common: Update from gnome-settings-daemon')

script_name = 'update-common-from-gsd'

script = configure_file(
  input: update_from_gsd_in,
  output: script_name + '.sh',
  configuration: script_conf
)

run_target(
  script_name,
  command: script
)

