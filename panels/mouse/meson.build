panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

desktop_in = configure_file(
  input: desktop + '.in.in',
  output: desktop + '.in',
  configuration: desktop_conf
)

i18n.merge_file(
  type: 'desktop',
  input: desktop_in,
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: control_center_desktopdir
)

resource_data = files(
  'cc-mouse-panel.ui',
  'cc-mouse-test.ui',
  'scroll-test-gegl.svg',
  'scroll-test.svg'
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
