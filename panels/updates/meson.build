panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

desktop_in = configure_file(
  input: desktop + '.in.in',
  output: desktop + '.in',
  configuration: desktop_conf,
)

i18n.merge_file(
  desktop,
  type: 'desktop',
  input: desktop_in,
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: control_center_desktopdir,
)

resource_data = files(
  'cc-updates-panel.ui',
  'cc-tariff-editor.ui',
)

common_sources = gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  dependencies: resource_data,
  export: true,
)

sources = common_sources + files(
  'cc-tariff-editor.c',
  'cc-updates-panel.c',
)

deps = common_deps + [
  dependency('libnm', version: '>= 1.8.0'),
  dependency('libnma', version: '>= 1.8.0'),
  dependency('mogwai-tariff-0', version: '>=0.1.0'),
]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
)
