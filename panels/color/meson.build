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

sources = files(
  'cc-color-panel.c',
  'cc-color-calibrate.c',
  'cc-color-cell-renderer-text.c',
  'cc-color-common.c',
  'cc-color-device.c',
  'cc-color-profile.c'
)

resource_data = files(
  'color-calibrate.ui',
  'color.ui'
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  dependencies: resource_data,
  export: true
)

deps = common_deps + [
  colord_dep,
  gnome_desktop_dep,
  m_dep,
  dependency('colord-gtk', version: '>= 0.1.24'),
  dependency('libsoup-2.4')
]

cflags += [
  '-DGNOMELOCALEDIR="@0@"'.format(control_center_localedir),
  '-DBINDIR="@0@"'.format(control_center_bindir)
]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags
)

subdir('icons')
