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
  'cc-ua-panel.c',
  'zoom-options.c'
)

resource_data = files(
  'cc-ua-panel.ui',
  'left_ptr_24px.png',
  'left_ptr_32px.png',
  'left_ptr_48px.png',
  'left_ptr_64px.png',
  'left_ptr_96px.png',
  'zoom-options.ui'
)

sources += gnome.compile_resources(
  'cc-ua-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname.underscorify(),
  dependencies: resource_data,
  export: true
)

deps = common_deps + [m_dep]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc ],
  dependencies: deps,
  c_args: cflags
)
