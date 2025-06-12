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

install_data(
  'noise-texture-light.png',
  install_dir: join_paths(control_center_pkgdatadir, 'pixmaps')
)

common_sources = []

common_sources += gnome.mkenums_simple(
  'cc-background-enum-types',
  sources: ['cc-background-item.h', 'cc-background-paintable.h']
)

gdesktop_enums_header = files(
  gsettings_desktop_dep.get_variable(pkgconfig: 'prefix') + '/include/gsettings-desktop-schemas/gdesktop-enums.h'
)

common_sources += gnome.mkenums_simple(
  'gdesktop-enums-types',
  sources: gdesktop_enums_header,
  identifier_prefix: 'GDesktop',
  symbol_prefix: 'g_desktop'
)

ui_sources = files(
  'cc-background-chooser.blp',
  'cc-background-panel.blp',
  'cc-background-preview.blp',
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

common_sources += gnome.compile_resources(
  'cc-@0@-resources'.format(cappletname),
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  export: true,
  dependencies: blueprints,
)

sources = common_sources + files(
  'bg-recent-source.c',
  'bg-source.c',
  'bg-wallpapers-source.c',
  'cc-background-chooser.c',
  'cc-background-item.c',
  'cc-background-paintable.c',
  'cc-background-panel.c',
  'cc-background-preview.c',
  'cc-background-xml.c',
)

deps = common_deps + [
  gdk_pixbuf_dep,
  gnome_bg_dep,
  libxml_dep,
  dependency('cairo-gobject'),
]

cflags += [
  '-DDATADIR="@0@"'.format(control_center_datadir),
  '-DGNOME_DESKTOP_USE_UNSTABLE_API'
]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags,
)

subdir('icons')
