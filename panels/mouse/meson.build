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

ui_sources = files(
  'cc-mouse-panel.blp',
  'cc-mouse-test.blp',
)

ui_compiled = []
foreach ui_source : ui_sources
  ui_compiled += fs.name(fs.replace_suffix(ui_source, '.ui'))
endforeach

blueprints = custom_target(
  input: ui_sources,
  output: ui_compiled,
  command: [blueprint_compiler, 'batch-compile', '@OUTDIR@', '@CURRENT_SOURCE_DIR@', '@INPUT@']
)

common_sources = gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  export: true,
  dependencies: blueprints,
)

sources = common_sources + files(
  'cc-mouse-panel.c',
  'cc-mouse-caps-helper.c',
  'cc-mouse-test.c',
)

deps = common_deps + [
  gnome_settings_dep,
  libdevice_dep,
]
if have_x11_support
  deps += [x11_dep, xi_dep]
endif

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
