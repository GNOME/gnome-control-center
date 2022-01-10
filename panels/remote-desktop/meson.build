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

sources = files('cc-remote-desktop-panel.c')

resource_data = files('cc-remote-desktop-panel.ui')

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname.underscorify (),
  dependencies: resource_data,
  export: true
)

cflags += '-DGNOMELOCALEDIR="@0@"'.format(control_center_localedir)

deps = [
  dependency('gcr-base-3'),
  dependency('libsecret-1'),
]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [top_inc, common_inc],
  dependencies: common_deps + deps,
  c_args: cflags
)
