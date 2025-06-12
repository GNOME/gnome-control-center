common_inc = include_directories('.')

common_sources = []

common_sources += gnome.mkenums_simple(
  'gsd-common-enums',
  sources: files('gsd-device-manager.h')
)

common_sources += gnome.mkenums_simple('cc-number-row-enums', sources: files('cc-number-row.h'))
common_sources += gnome.mkenums_simple('cc-timelike-editor-enums', sources: files('cc-timelike-editor.h'))

ui_sources = files(
  'cc-content-row.blp',
  'cc-duration-editor.blp',
  'cc-duration-row.blp',
  'cc-illustrated-row.blp',
  'cc-language-chooser.blp',
  'cc-language-row.blp',
  'cc-list-row-info-button.blp',
  'cc-list-row.blp',
  'cc-permission-infobar.blp',
  'cc-split-row.blp',
  'cc-time-editor.blp',
  'cc-timelike-editor.blp',
)

ui_compiled = []
foreach ui_source : ui_sources
  ui_compiled += fs.name(fs.replace_suffix(ui_source, '.ui'))
endforeach

blueprints = custom_target(
  input: ui_sources,
  output: ui_compiled,
  command: [blueprint_compiler, 'batch-compile', '@OUTDIR@', '@CURRENT_SOURCE_DIR@', '@INPUT@']
)

common_sources += gnome.compile_resources(
  'cc-common-resources',
  'common.gresource.xml',
  c_name: 'cc_common',
  export: true,
  dependencies: blueprints,
)

generates_sources_dep = declare_dependency(
  sources: common_sources,
)

sources = files(
  'cc-hostname-entry.c',
  'cc-number-row.c',
  'cc-timelike-entry.c',
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
  'cc-duration-editor.c',
  'cc-duration-row.c',
  'cc-hostname.c',
  'cc-illustrated-row.c',
  'cc-language-chooser.c',
  'cc-language-row.c',
  'cc-list-row.c',
  'cc-list-row-info-button.c',
  'cc-mask-paintable.c',
  'cc-time-editor.c',
  'cc-timelike-editor.c',
  'cc-timelike-editor-layout.c',
  'cc-permission-infobar.c',
  'cc-split-row.c',
  'cc-texture-utils.c',
  'cc-content-row.c',
  'cc-ui-util.c',
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
