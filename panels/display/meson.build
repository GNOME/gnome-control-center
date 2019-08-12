panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

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

sources = files(
  'cc-display-panel.c',
  'cc-display-arrangement.c',
  'cc-display-config.c',
  'cc-display-config-dbus.c',
  'cc-display-config-manager-dbus.c',
  'cc-display-config-manager.c',
  'cc-display-settings.c',
  'cc-night-light-page.c',
)

resource_data = files(
  'cc-display-panel.ui',
  'cc-display-settings.ui',
  'cc-night-light-page.ui',
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  source_dir: ['.', 'icons'],
  c_name: 'cc_' + cappletname,
  dependencies: resource_data,
  export: true
)

deps = common_deps + [
  colord_dep,
  gnome_desktop_dep,
  m_dep,
  upower_glib_dep
]

cflags += [
  '-DDATADIR="@0@"'.format(control_center_datadir)
]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc ],
  dependencies: deps,
  c_args: cflags
)

subdir('icons')
