name = 'wireless-security'

wireless_security_inc = include_directories('.')

nm_applet_headers = [
  'eap-method.h',
  'eap-method-fast.h',
  'eap-method-leap.h',
  'eap-method-peap.h',
  'eap-method-simple.h',
  'eap-method-tls.h',
  'eap-method-ttls.h',
  'helpers.h',
  'wireless-security.h',
  'ws-leap.h',
  'ws-dynamic-wep.h',
  'ws-wep-key.h',
  'ws-wpa-eap.h',
  'ws-wpa-psk.h'
]

nm_applet_sources = [
  'eap-method.c',
  'eap-method-fast.c',
  'eap-method-leap.c',
  'eap-method-peap.c',
  'eap-method-simple.c',
  'eap-method-tls.c',
  'eap-method-ttls.c',
  'helpers.c',
  'wireless-security.c',
  'ws-leap.c',
  'ws-dynamic-wep.c',
  'ws-wep-key.c',
  'ws-wpa-eap.c',
  'ws-wpa-psk.c'
]

sources = files(nm_applet_sources) + files('utils.c')

nm_resource_data = [
  'eap-method-fast.ui',
  'eap-method-leap.ui',
  'eap-method-peap.ui',
  'eap-method-simple.ui',
  'eap-method-tls.ui',
  'eap-method-ttls.ui',
  'ws-dynamic-wep.ui',
  'ws-leap.ui',
  'ws-wep-key.ui',
  'ws-wpa-eap.ui',
  'ws-wpa-psk.ui'
]

sources += gnome.compile_resources(
  name + '-resources',
  name + '.gresource.xml',
  c_name: name.underscorify(),
  dependencies: files(nm_resource_data),
  export: true
)

libwireless_security = static_library(
  name,
  sources: sources,
  include_directories: top_inc,
  dependencies: deps
)

'''
# DISABLED
# FIXME: workaround for updating different sources code
input_dir = join_paths(meson.source_root(), '..', 'network-manager-applet', 'src', 'wireless-security')

script_conf = configuration_data()
script_conf.set('program', update_from_gsd.path())
script_conf.set('input_dir', input_dir)
script_conf.set('working_dir', meson.current_source_dir())
script_conf.set('source_files', ' '.join(nm_applet_sources + nm_applet_headers))
script_conf.set('source_patch', 'nm-connection-editor-to-network-panel.patch')
script_conf.set('source_message', 'network: Update wireless-security from network-manager-applet')
script_conf.set('resource_data', ' '.join(nm_resource_data))
script_conf.set('resource_patch', 'nm-connection-editor-ui-to-network-panel.patch')
script_conf.set('resource_message', 'network: Update wireless-security UI from network-manager-applet')

script_name = 'update-from-nma'

script = configure_file(
  input: update_from_nma_in,
  output: script_name + '.sh',
  configuration: script_conf
)

run_target(
  script_name,
  command: script
)
'''
