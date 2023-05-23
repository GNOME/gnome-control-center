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

sources = files(
  'bolt-client.c',
  'bolt-device.c',
  'bolt-enums.c',
  'bolt-error.c',
  'bolt-names.c',
  'bolt-proxy.c',
  'bolt-str.c',
  'bolt-time.c',
  'cc-bolt-panel.c',
  'cc-bolt-device-dialog.c',
  'cc-bolt-device-entry.c',
)

enum_headers = [
  'bolt-enums.h',
  'bolt-error.h'
]

sources += gnome.mkenums_simple(
  'bolt-enum-types',
  sources: enum_headers)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  source_dir: '.',
  c_name: 'cc_' + cappletname,
  export: true
)

deps = common_deps + [
  polkit_gobject_dep,
  m_dep,
]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [top_inc, common_inc],
  dependencies: deps,
  c_args: cflags
)

subdir('icons')
