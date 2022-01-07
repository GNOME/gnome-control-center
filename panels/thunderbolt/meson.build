panels_list += cappletname

desktop = 'gnome-@0@-panel.desktop'.format(cappletname)
desktop_in = configure_file(
  input: desktop + '.in.in',
  output: desktop + '.in',
  configuration: desktop_conf
)

i18n.merge_file(
  type: 'desktop',
  input: desktop_in,
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

resource_data = files(
  'cc-bolt-device-dialog.ui',
  'cc-bolt-device-entry.ui',
  'cc-bolt-panel.ui'
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  source_dir: '.',
  c_name: 'cc_' + cappletname,
  dependencies: resource_data,
  export: true
)

deps = common_deps + [
  gnome_desktop_dep,
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
