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
  'network-ethernet.ui',
  'network-mobile.ui',
  'network-proxy.ui',
  'network-simple.ui',
  'network.ui',
  'network-vpn.ui',
  'network-wifi.ui',
  'wifi.ui'
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  dependencies: resource_data,
  export: true
)

cflags += '-DGNOMELOCALEDIR="@0@"'.format(control_center_localedir)

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags,
  link_with: libconnection_editor
)


test_cflags = cflags
test_cflags += [
  '-DNETWORKMANAGER_COMPILATION=NM_NETWORKMANAGER_COMPILATION_WITH_GLIB',
  '-DTEST_NM_SERVICE="@0@"'.format(join_paths(meson.source_root(), 'panels', 'network', 'nm-utils', 'test-networkmanager-service.py')),
]

test_sources = sources + shell_test_sources
test_sources += files('nm-utils/nm-test-utils-impl.c')
test_sources += files('test-network-panel.c')

exe = executable(
  'test-network-panel',
  sources: test_sources,
  include_directories: [top_inc, include_directories('nm-utils')],
  dependencies: shell_deps + deps,
  c_args: test_cflags,
  link_with: libconnection_editor
)

envs = [
#  'G_DEBUG=fatal_warnings',
  'G_MESSAGES_DEBUG=all',
  'BUILDDIR=' + meson.current_build_dir(),
  'TOP_BUILDDIR=' + meson.build_root()
]

test('test-network-panel',
  find_program('test-network-panel.py'),
  env: envs,
  timeout: 10
)
