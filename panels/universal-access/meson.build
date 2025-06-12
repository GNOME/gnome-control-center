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
  'cc-cursor-size-page.blp',
  'cc-ua-hearing-page.blp',
  'cc-ua-mouse-page.blp',
  'cc-ua-panel.blp',
  'cc-ua-seeing-page.blp',
  'cc-ua-typing-page.blp',
  'cc-ua-zoom-page.blp',
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
  'cc-cursor-size-page.c',
  'cc-ua-panel.c',
  'cc-ua-hearing-page.c',
  'cc-ua-mouse-page.c',
  'cc-ua-seeing-page.c',
  'cc-ua-typing-page.c',
  'cc-ua-zoom-page.c',
)

sources += gnome.compile_resources(
  'cc-ua-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname.underscorify(),
  export: true,
  dependencies: blueprints,
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
