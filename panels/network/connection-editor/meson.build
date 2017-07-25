name = 'connection-editor'

sources = files(
  'ce-page-8021x-security.c',
  'ce-page-details.c',
  'ce-page-ethernet.c',
  'ce-page-ip4.c',
  'ce-page-ip6.c',
  'ce-page-security.c',
  'ce-page-vpn.c',
  'ce-page-wifi.c',
  'ce-page.c',
  'net-connection-editor.c',
  'ui-helpers.c',
  'vpn-helpers.c'
)

resource_data = files(
  '8021x-security-page.ui',
  'connection-editor.ui',
  'details-page.ui',
  'ethernet-page.ui',
  'ip4-page.ui',
  'ip6-page.ui',
  'security-page.ui',
  'vpn-page.ui',
  'wifi-page.ui'
)

c_name = 'net-' + name

sources += gnome.compile_resources(
  c_name + '-resources',
  name + '.gresource.xml',
  c_name: c_name.underscorify(),
  dependencies: resource_data,
  export: true
)

ce_cflags = cflags + [
  '-DNM_VPN_CONFIG_DIR="@0@"'.format(nm_vpn_config_dir),
  '-DNM_VPN_MODULE_DIR="@0@"'.format(nm_vpn_module_dir)
]

libconnection_editor = static_library(
  name,
  sources: sources,
  include_directories: [top_inc, wireless_security_inc],
  dependencies: deps,
  c_args: ce_cflags,
  link_with: libwireless_security
)
