subdir('appdata')
subdir('completions')

service_conf = configuration_data()
service_conf.set('bindir', control_center_bindir)

service = 'org.gnome.ControlCenter.service'

configure_file(
  input: service + '.in',
  output: service,
  install: true,
  install_dir: join_paths(control_center_datadir, 'dbus-1', 'services'),
  configuration: service_conf
)

desktop = 'gnome-control-center.desktop'

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

cflags = ['-DGNOMELOCALEDIR="@0@"'.format(control_center_localedir)]

libshell = static_library(
  'shell',
  sources: 'cc-shell-model.c',
  include_directories: [top_inc, common_inc],
  dependencies: common_deps,
  c_args: cflags
)

common_sources = files(
  'cc-application.c',
  'cc-editable-entry.c',
  'cc-hostname-entry.c',
  'cc-panel-loader.c',
  'cc-panel.c',
  'cc-shell-category-view.c',
  'cc-shell-item-view.c',
  'cc-shell-log.c',
  'cc-shell.c',
  'hostname-helper.c',
  'list-box-helper.c',
  'main.c'
)

resource_data = files(
  'help-overlay.ui',
  'panel-list.ui',
  'window.ui'
)

common_sources += gnome.compile_resources(
  'resources',
  meson.project_name() + '.gresource.xml',
  dependencies: resource_data,
  export: true
)

sources = common_sources + files(
  'cc-panel-list.c',
  'cc-window.c'
)

shell_deps = common_deps + [
  libdevice_dep,
  libgd_dep,
  liblanguage_dep,
  polkit_gobject_dep,
  x11_dep
]

if enable_cheese
  shell_deps += cheese_deps
endif

if enable_linux
  shell_deps += wacom_deps
endif

executable(
  meson.project_name(),
  sources,
  include_directories: top_inc,
  dependencies: shell_deps,
  c_args: cflags,
  link_with: panels_libs + [libshell],
  install: true
)

# Because it is confusing and somewhat problematic to directly add and compile
# cc-panel-loader.o by another directory (i.e. the shell search provider), we
# have to create a library and link it there, just like libshell.la.
libpanel_loader = static_library(
  'panel_loader',
  sources: 'cc-panel-loader.c',
  include_directories: top_inc,
  dependencies: common_deps,
  c_args: cflags + ['-DCC_PANEL_LOADER_NO_GTYPES']
)

test_unit = 'test-hostname'

sources = files(
  'hostname-helper.c',
  test_unit + '.c'
)

cflags += [
  '-DTEST_SRCDIR="@0@"'.format(meson.current_source_dir()),
  '-DTEST_TOPSRCDIR="@0@"'.format(meson.source_root())
]

exe = executable(
  test_unit,
  sources,
  include_directories: top_inc,
  dependencies: common_deps,
  c_args: cflags
)

test(test_unit, exe)
