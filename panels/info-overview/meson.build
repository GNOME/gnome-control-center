panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

i18n.merge_file(
  type: 'desktop',
  input: desktop + '.in',
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: control_center_desktopdir
)

cflags += [
  '-DDATADIR="@0@"'.format(control_center_datadir),
  '-DLIBEXECDIR="@0@"'.format(control_center_libexecdir),
]

sources = files(
  'cc-info-overview-panel.c',
  'info-cleanup.c'
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname.underscorify (),
  export: true
)

deps = common_deps + [
  dependency('udisks2', version: '>= 2.8.2'),
  dependency('libgtop-2.0'),
  gudev_dep,
]

info_panel_lib = static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc ],
  dependencies: deps,
  c_args: cflags
)
panels_libs += info_panel_lib

print_renderer_deps = [
  dependency('gtk4'),
  epoxy_dep
]

executable(
  'test-info-cleanup',
  [ 'info-cleanup.c', 'test-info-cleanup.c' ],
  include_directories: [ top_inc ],
  dependencies: [ print_renderer_deps ],
  c_args: cflags,
)

executable(
  'gnome-control-center-print-renderer',
  'gnome-control-center-print-renderer.c',
  include_directories: [ top_inc ],
  dependencies: [ print_renderer_deps ],
  c_args: cflags,
  install: true,
  install_dir: control_center_libexecdir,
)

install_data(
  [ 'gnome-logo-text.svg', 'gnome-logo-text-dark.svg' ],
  install_dir: join_paths(control_center_datadir, 'icons')
)

subdir('icons')
