panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

desktop_in = configure_file(
  input: desktop + '.in.in',
  output: desktop + '.in',
  configuration: desktop_conf
)

custom_target(
  desktop,
  input: desktop_in,
  output: desktop,
  command: intltool_desktop_cmd,
  install: true,
  install_dir: control_center_desktopdir
)

polkit_conf = configuration_data()
polkit_conf.set('libexecdir', control_center_libexecdir)

polkit = 'org.gnome.controlcenter.remote-login-helper.policy'

polkit_in = configure_file(
  input: polkit + '.in.in',
  output: polkit + '.in',
  configuration: polkit_conf
)

custom_target(
  polkit,
  input: polkit_in,
  output: polkit,
  command: intltool_xml_cmd,
  install: true,
  install_dir: join_paths(control_center_datadir, 'polkit-1', 'actions')
)

sources = files(
  'cc-sharing-panel.c',
  'cc-media-sharing.c',
  'cc-remote-login.c',
  'cc-sharing-networks.c',
  'cc-sharing-switch.c',
  'file-share-properties.c',
  'vino-preferences.c'
)

resource_data = files(
  'networks.ui',
  'sharing.ui'
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  dependencies: resource_data,
  export: true
)

settings_daemon = 'org.gnome.SettingsDaemon'
gdbus = settings_daemon + '.Sharing'

sources += gnome.gdbus_codegen(
  gdbus,
  gdbus + '.xml',
  interface_prefix: settings_daemon + '.',
  namespace: 'Gsd'
)

cflags += [
  '-DLIBEXECDIR="@0@"'.format(control_center_libexecdir),
  '-DSYSCONFDIR="@0@"'.format(control_center_sysconfdir)
]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: top_inc,
  dependencies: common_deps,
  c_args: cflags
)

name = 'cc-remote-login-helper'

deps = [
  gio_dep,
  glib_dep
]

executable(
  name,
  name + '.c',
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags,
  install: true,
  install_dir: control_center_libexecdir
)
