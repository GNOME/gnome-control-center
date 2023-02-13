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

resource_data = files(
  'cc-mouse-panel.ui',
  'cc-mouse-test.ui',
  'scroll-test-gegl.svg',
  'scroll-test.svg',
  'assets/edge-scroll-d.webm',
  'assets/edge-scroll-l.webm',
  'assets/scroll-2finger-d.webm',
  'assets/scroll-2finger-l.webm',
  'assets/scroll-traditional-d.webm',
  'assets/scroll-traditional-l.webm',
  'assets/scroll-natural-d.webm',
  'assets/scroll-natural-l.webm',
  'assets/touch-scroll-natural-d.webm',
  'assets/touch-scroll-natural-l.webm',
  'assets/touch-scroll-traditional-d.webm',
  'assets/touch-scroll-traditional-l.webm',
  'assets/tap-to-click-l.webm',
  'assets/tap-to-click-d.webm',
)

common_sources = gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  dependencies: resource_data,
  export: true
)

sources = common_sources + files(
  'cc-mouse-panel.c',
  'cc-mouse-caps-helper.c',
  'cc-mouse-test.c',
)

deps = common_deps + [
  gnome_settings_dep,
  libdevice_dep,
  x11_dep,
  xi_dep
]

panels_libs += static_library(
  cappletname + '-properties',
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags
)

test_name = 'test-gnome-mouse-test'

sources = common_sources + files(
  'cc-mouse-test.c',
  test_name + '.c'
)

executable(
  test_name,
  sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags
)

subdir('icons')
