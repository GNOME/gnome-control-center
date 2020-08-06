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
  'cc-battery-row.c',
  'cc-brightness-scale.c',
  'cc-power-panel.c',
  'cc-power-profile-row.c',
)

sources += gnome.mkenums_simple(
  'cc-brightness-scale-types',
  sources: ['cc-brightness-scale.h'])

resource_data = files(
  'cc-battery-row.ui',
  'cc-power-panel.ui'
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  dependencies: resource_data,
  export: true
)

deps = common_deps + [
  gnome_settings_dep,
  upower_glib_dep
]

if host_is_linux
  deps += network_manager_deps
endif

if host_is_linux_not_s390
  deps += gnome_bluetooth_dep
endif

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc ],
  dependencies: deps,
  c_args: cflags
)

subdir('icons')
