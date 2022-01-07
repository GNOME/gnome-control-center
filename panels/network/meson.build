deps = common_deps + network_manager_deps + [
  polkit_gobject_dep,
  dependency('gmodule-2.0')
]

network_inc = include_directories('.')

subdir('wireless-security')
subdir('connection-editor')

panel_names = [
  cappletname,
  'wifi'
]

panels_list += panel_names

foreach name: panel_names
  desktop = 'gnome-' + name + '-panel.desktop'

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
endforeach

sources = files(
  'cc-qr-code.c',
  'cc-network-panel.c',
  'cc-wifi-connection-row.c',
  'cc-wifi-connection-list.c',
  'cc-wifi-panel.c',
  'cc-wifi-hotspot-dialog.c',
  'net-device-bluetooth.c',
  'net-device-ethernet.c',
  'net-device-mobile.c',
  'net-device-wifi.c',
  'net-proxy.c',
  'net-vpn.c',
  'network-dialogs.c',
  'panel-common.c',
  'ui-helpers.c'
)

resource_data = files(
  'cc-network-panel.ui',
  'cc-wifi-connection-row.ui',
  'cc-wifi-panel.ui',
  'cc-wifi-hotspot-dialog.ui',
  'network-bluetooth.ui',
  'network-ethernet.ui',
  'network-mobile.ui',
  'network-proxy.ui',
  'network-vpn.ui',
  'network-wifi.ui',
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  dependencies: resource_data,
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
