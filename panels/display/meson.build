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
  'cc-display-panel.blp',
  'cc-display-settings.blp',
  'cc-night-light-page.blp',
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

sources = files(
  'cc-display-panel.c',
  'cc-display-arrangement.c',
  'cc-display-config.c',
  'cc-display-config-manager.c',
  'cc-display-settings.c',
  'cc-night-light-page.c',
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  source_dir: ['.', 'icons'],
  c_name: 'cc_' + cappletname,
  export: true,
  dependencies: blueprints,
)

deps = common_deps + [
  colord_dep,
  m_dep,
  upower_glib_dep
]

cflags += [
  '-DDATADIR="@0@"'.format(control_center_datadir)
]

display_panel_lib = static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc ],
  dependencies: deps,
  c_args: cflags
)

panels_libs += display_panel_lib

subdir('icons')
