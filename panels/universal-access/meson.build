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

sources = files(
  'cc-cursor-size-dialog.c',
  'cc-ua-panel.c',
  'cc-ua-hearing-page.c',
  'cc-ua-mouse-page.c',
  'cc-ua-seeing-page.c',
  'cc-ua-typing-page.c',
  'cc-ua-zoom-page.c',
)

resource_data = files(
  'cc-ua-panel.ui',
  'hearing-symbolic.svg',
  'pointing-symbolic.svg',
  'seeing-symbolic.svg',
  'typing-symbolic.svg',
  'zoom-symbolic.svg',
  'left_ptr_24px.png',
  'left_ptr_32px.png',
  'left_ptr_48px.png',
  'left_ptr_64px.png',
  'left_ptr_96px.png',
  'cc-cursor-size-dialog.ui',
  'cc-ua-hearing-page.ui',
  'cc-ua-mouse-page.ui',
  'cc-ua-seeing-page.ui',
  'cc-ua-typing-page.ui',
  'cc-ua-zoom-page.ui',
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

subdir('icons')
