common_inc = include_directories('.')

common_sources = []

common_sources += gnome.mkenums_simple(
  'gsd-common-enums',
  sources: files('gsd-device-manager.h')
)

common_sources += gnome.mkenums_simple('cc-number-row-enums', sources: files('cc-number-row.h'))

common_sources += gnome.compile_resources(
  'cc-common-resources',
  'common.gresource.xml',
  c_name: 'cc_common',
  export: true
)

generates_sources_dep = declare_dependency(
  sources: common_sources,
)

sources = files(
  'cc-hostname-entry.c',
  'cc-number-row.c',
  'cc-time-entry.c',
  'cc-util.c',
  'hostname-helper.c'
)

libwidgets = static_library(
  'widgets',
  sources: sources,
  include_directories: top_inc,
  dependencies: common_deps + [ generates_sources_dep, polkit_gobject_dep ]
)
libwidgets_dep = declare_dependency(
  include_directories: common_inc,
  link_with: libwidgets
)

sources = common_sources + files(
  'cc-common-language.c',
  'cc-hostname.c',
  'cc-illustrated-row.c',
  'cc-language-chooser.c',
  'cc-language-row.c',
  'cc-list-row.c',
  'cc-list-row-info-button.c',
  'cc-mask-paintable.c',
  'cc-time-editor.c',
  'cc-permission-infobar.c',
  'cc-split-row.c',
  'cc-texture-utils.c',
  'cc-vertical-row.c',
  'cc-util.c',
  'cc-scaler.c'
)

deps = common_deps + [
  generates_sources_dep,
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
  'gsd-input-helper.h'
]

gsd_sources = [
  'gsd-device-manager.c',
  'gsd-input-helper.c'
]

sources = common_sources + files(gsd_sources)

deps = common_deps + [ gudev_dep ]

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

polkit_conf = configuration_data()
polkit_conf.set('PRIVILEGED_GROUP', get_option('privileged_group'))
configure_file(
  input: 'gnome-control-center.rules.in',
  output: 'gnome-control-center.rules',
  configuration: polkit_conf,
  install_dir: join_paths(control_center_datadir, 'polkit-1', 'rules.d')
)
