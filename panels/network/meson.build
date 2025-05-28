deps = common_deps + network_manager_deps + [
  polkit_gobject_dep,
  dependency('gmodule-2.0')
]

network_inc = include_directories('.')

subdir('connection-editor')
subdir('icons')

panel_names = [
  cappletname,
  'wifi'
]

panels_list += panel_names

foreach name: panel_names
  desktop = 'gnome-' + name + '-panel.desktop'

  i18n.merge_file(
    type: 'desktop',
    input: desktop + '.in',
    output: desktop,
    po_dir: po_dir,
    install: true,
    install_dir: control_center_desktopdir
  )
endforeach

ui_sources = files(
  'cc-net-proxy-page.blp',
  'cc-network-panel.blp',
  'cc-qr-code-dialog.blp',
  'cc-wifi-connection-row.blp',
  'cc-wifi-hotspot-dialog.blp',
  'cc-wifi-panel.blp',
  'network-bluetooth.blp',
  'network-ethernet.blp',
  'network-vpn.blp',
  'network-wifi.blp',
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

sources = files(
  'cc-qr-code.c',
  'cc-qr-code-dialog.c',
  'cc-network-panel.c',
  'cc-net-proxy-page.c',
  'cc-wifi-connection-row.c',
  'cc-wifi-connection-list.c',
  'cc-wifi-panel.c',
  'cc-wifi-hotspot-dialog.c',
  'net-device-bluetooth.c',
  'net-device-ethernet.c',
  'net-device-mobile.c',
  'net-device-wifi.c',
  'net-vpn.c',
  'network-dialogs.c',
  'panel-common.c',
  'ui-helpers.c'
)

resources_ch = gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  export: true,
  dependencies: blueprints,
)

sources += resources_ch

network_panel_lib = static_library(
  cappletname,
  sources: sources,
  include_directories: [top_inc, common_inc],
  dependencies: deps,
  c_args: cflags,
  link_with: libconnection_editor
)
panels_libs += network_panel_lib

network_panel_dep = declare_dependency(
  include_directories: [top_inc, network_inc],
  sources: [resources_ch[1]],
  link_with: network_panel_lib,
)
