name = 'connection-editor'

ui_sources = files(
  'connection-editor.blp',
  'details-page.blp',
  'security-page.blp',
  '8021x-security-page.blp',
  'bluetooth-page.blp',
  'ethernet-page.blp',
  'ip4-page.blp',
  'ip6-page.blp',
  'vpn-page.blp',
  'wifi-page.blp',
  'wireguard-page.blp',
  'wireguard-peer.blp',
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
  'ce-ip-address-entry.c',
  'ce-netmask-entry.c',
  'ce-page-8021x-security.c',
  'ce-page-bluetooth.c',
  'ce-page-details.c',
  'ce-page-ethernet.c',
  'ce-page-ip4.c',
  'ce-page-ip6.c',
  'ce-page-security.c',
  'ce-page-vpn.c',
  'ce-page-wireguard.c',
  'ce-page-wifi.c',
  'ce-page.c',
  'net-connection-editor.c',
  'vpn-helpers.c'
)

c_name = 'net-' + name

sources += gnome.compile_resources(
  c_name + '-resources',
  name + '.gresource.xml',
  c_name: c_name.underscorify(),
  export: true,
  dependencies: blueprints,
)

libconnection_editor = static_library(
  name,
  sources: sources,
  include_directories: [top_inc, common_inc, network_inc],
  dependencies: deps,
  c_args: cflags,
)
