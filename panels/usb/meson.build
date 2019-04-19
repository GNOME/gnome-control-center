panels_list += cappletname

statedir = get_option('localstatedir')
usbdbdir = join_paths('/', statedir, 'lib/usb')

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
  'usb-io.c',
  'usb-store.c'
)

resource_data = files(
  'cc-usb-device-entry.ui',
  'cc-usb-panel.ui'
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
  '-DUSBDBDIR="@0@"'.format(usbdbdir),
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

udev_conf = configuration_data()
udev_conf.set_quoted('top_builddir', meson.build_root())
udev_conf.set_quoted('top_srcdir', meson.source_root())

udev_rule = configure_file(
  output: '70-gnome-usb.rules',
  input: '70-gnome-usb.rules.in',
  configuration: udev_conf
)

install_data(
  udev_rule,
  install: true,
  install_dir: udev_rules_dir
)

name = 'cc-usb-device-helper'

executable(
  name,
  name + '.c',
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags,
  link_with: panels_libs,
  install: true,
  install_dir: control_center_libexecdir
)
