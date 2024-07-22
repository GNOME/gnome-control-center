deps = common_deps + network_manager_deps + [
  polkit_gobject_dep,
  dependency('gmodule-2.0')
]

network_inc = include_directories('.')

subdir('wwan')
subdir('connection-editor')
subdir('icons')

panels_list += cappletname

desktop = 'gnome-' + cappletname + '-panel.desktop'
i18n.merge_file(
  type: 'desktop',
  input: desktop + '.in',
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: control_center_desktopdir
)

sources = files(
  'cc-qr-code.c',
  'cc-qr-code-dialog.c',
  'cc-network-panel.c',
  'cc-net-proxy-page.c',
  'cc-vpn-page.c',
  'cc-wifi-connection-row.c',
  'cc-wifi-connection-list.c',
  'cc-wifi-hotspot-dialog.c',
  'cc-wifi-page.c',
  'net-device-bluetooth.c',
  'net-device-ethernet.c',
  'net-device-mobile.c',
  'net-device-wifi.c',
  'net-vpn.c',
  'network-dialogs.c',
  'panel-common.c',
  'ui-helpers.c'
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  export: true
)

network_panel_lib = static_library(
  cappletname,
  sources: sources,
  include_directories: [top_inc, common_inc],
  dependencies: deps,
  c_args: cflags,
  link_with: libconnection_editor
)
panels_libs += network_panel_lib
