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
  '-DLIBEXECDIR="@0@"'.format(control_center_libexecdir),
]

sources = files(
  'cc-online-account-provider-row.c',
  'cc-online-account-row.c',
  'cc-online-accounts-panel.c',
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname.underscorify(),
  export: true
)

deps = common_deps + [
  goa_dep,
]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc ],
  dependencies: deps,
  c_args: cflags
)

goa_helper_deps = [
  dependency('goa-backend-1.0', version: goa_req_version),
]

goa_helper_cflags = cflags + [
  '-DGNOMELOCALEDIR="@0@"'.format(control_center_localedir),
]

gtk_x11_dep = dependency('gtk+-x11-3.0', required: false)
if gtk_x11_dep.found()
  goa_helper_cflags += ['-DHAVE_GTK_X11']
endif

gtk_wayland_dep = dependency('gtk+-wayland-3.0', required: false)
if gtk_wayland_dep.found()
  goa_helper_cflags += ['-DHAVE_GTK_WAYLAND']
endif

executable(
  'gnome-control-center-goa-helper',
  'gnome-control-center-goa-helper.c',
  include_directories: [ top_inc ],
  dependencies: goa_helper_deps,
  c_args: goa_helper_cflags,
  install: true,
  install_dir: control_center_libexecdir,
)

subdir('icons')
