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
  'cc-camera-page.c',
  'cc-diagnostics-page.c',
  'cc-location-page.c',
  'cc-microphone-page.c',
  'cc-privacy-panel.c',
  'cc-screen-page.c',
  'cc-usage-page.c',
)

sources += gnome.mkenums_simple(
  'cc-screen-page-enums',
  sources: ['cc-screen-page.h'],
)

sources += gnome.mkenums_simple(
  'cc-usage-page-enums',
  sources: ['cc-usage-page.h'],
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  export: true
)

cflags += [
  '-DDATADIR="@0@"'.format(control_center_datadir)
]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc ],
  dependencies: common_deps,
  c_args: cflags
)

subdir('icons')
