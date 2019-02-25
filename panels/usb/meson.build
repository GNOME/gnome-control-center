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

polkit = 'org.gnome.controlcenter.@0@.policy'.format(cappletname)

i18n.merge_file(
  polkit,
  input: polkit + '.in',
  output: polkit,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(control_center_datadir, 'polkit-1', 'actions')
)

sources = files(
  'cc-usb-panel.c',
  'cc-usb-device-entry.c',
  'usb-device.c',
  'gnome-usb-properties.c'
)

resource_data = files(
  'cc-usb-device-entry.ui',
  'gnome-usb-properties.ui'
)


sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  source_dir: '.',
  c_name: 'cc_' + cappletname,
  dependencies: resource_data,
  export: true
)

libudev = dependency('libudev')

deps = common_deps + [
  gnome_settings_dep,
  libdevice_dep,
  polkit_gobject_dep,
  x11_dep,
  xi_dep,
  libudev
]

#if enable_wayland
#  deps += wayland_deps
#endif

deps += wayland_deps

cflags += [
  '-DGNOMELOCALEDIR="@0@"'.format(control_center_localedir),
  '-DLIBEXECDIR="@0@"'.format(control_center_libexecdir)
]

panels_libs += static_library(
  cappletname + '-properties',
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags
)

name = 'usb-hwdb'

install_data(
  name + '.sh',
  install: true,
  install_dir: control_center_libexecdir
)