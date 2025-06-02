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

blueprints = custom_target(
  'blueprints',
  input: files(
    'cc-color-calibrate.blp',
    'cc-color-device.blp',
    'cc-color-profile.blp',
  ),
  output: '.',
  command: [
    blueprint_compiler,
    'batch-compile',
    '@OUTPUT@',
    '@CURRENT_SOURCE_DIR@',
    '@INPUT@',
  ],
)

sources = files(
  'cc-color-panel.c',
  'cc-color-calibrate.c',
  'cc-color-cell-renderer-text.c',
  'cc-color-common.c',
  'cc-color-device.c',
  'cc-color-profile.c'
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  export: true,
  dependencies: blueprints,
)

deps = common_deps + [
  colord_dep,
  gnome_rr_dep,
  m_dep,
  dependency('colord-gtk4', version: '>= 0.1.24'),
]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc ],
  dependencies: deps,
  c_args: cflags
)

subdir('icons')
