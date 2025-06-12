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
  'cc-sharing-networks.blp',
  'cc-sharing-panel.blp',
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
  'cc-sharing-panel.c',
  'cc-media-sharing.c',
  'cc-sharing-networks.c',
  'file-share-properties.c',
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  export: true,
  dependencies: blueprints,
)

settings_daemon = 'org.gnome.SettingsDaemon'
gdbus = settings_daemon + '.Sharing'

sources += gnome.gdbus_codegen(
  gdbus,
  gdbus + '.xml',
  interface_prefix: settings_daemon + '.',
  namespace: 'Gsd'
)

cflags += [
  '-DLIBEXECDIR="@0@"'.format(control_center_libexecdir),
  '-DSYSCONFDIR="@0@"'.format(control_center_sysconfdir)
]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc ],
  dependencies: [
    common_deps,
    polkit_gobject_dep,
  ],
  c_args: cflags
)

subdir('icons')
