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
  'cc-system-panel.c',
  'region/cc-region-page.c',
  'region/cc-format-chooser.c',
  'region/cc-format-preview.c',
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name : 'cc_' + cappletname,
  export : true
)

deps = common_deps + [
  accounts_dep,
  gnome_desktop_dep,
  liblanguage_dep,
  polkit_gobject_dep
]

if enable_ibus
  deps += ibus_dep
endif

subdir('region')

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc ],
  dependencies: deps,
  c_args: cflags
)
