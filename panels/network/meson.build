deps = common_deps + network_manager_deps + [
  polkit_gobject_dep,
  dependency('gmodule-2.0')
]

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
    desktop,
    type: 'desktop',
    input: desktop_in,
    output: desktop,
    po_dir: po_dir,
    install: true,
    install_dir: control_center_desktopdir
  )
endforeach

sources = files(
  'cc-network-panel.c',
  'cc-wifi-panel.c',
  'net-device.c',
  'net-device-ethernet.c',
  'net-device-mobile.c',
  'net-device-simple.c',
  'net-device-wifi.c',
  'net-object.c',
  'net-proxy.c',
  'net-vpn.c',
  'network-dialogs.c',
  'panel-common.c'
)

resource_data = files(
  'cc-network-panel.ui',
  'cc-wifi-panel.ui',
  'network-ethernet.ui',
  'network-mobile.ui',
  'network-proxy.ui',
  'network-simple.ui',
  'network-vpn.ui',
  'network-wifi.ui',
)

status_icons_files = [
  'entry-show-symbolic.svg'
]

foreach file: status_icons_files
  install_data(
    file,
    install_dir: join_paths(control_center_icondir, 'hicolor', 'scalable', 'status')
  )
endforeach

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  dependencies: resource_data,
  export: true
)

cflags += '-DGNOMELOCALEDIR="@0@"'.format(control_center_localedir)

network_panel_lib = static_library(
  cappletname,
  sources: sources,
  include_directories: [top_inc, common_inc],
  dependencies: deps,
  c_args: cflags,
  link_with: libconnection_editor
)
panels_libs += network_panel_lib