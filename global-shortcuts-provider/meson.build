service_conf = configuration_data()
service_conf.set('libexecdir', control_center_libexecdir)

service = 'org.gnome.Settings.GlobalShortcutsProvider.service'

configure_file(
  input: service + '.in',
  output: service,
  install: true,
  install_dir: join_paths(control_center_datadir, 'dbus-1', 'services'),
  configuration: service_conf
)

ui_sources = files(
  'cc-global-shortcut-dialog.blp',
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
  'cc-global-shortcut-dialog.c',
  'cc-global-shortcuts-provider.c',
  'control-center-global-shortcuts-provider.c'
)

global_shortcuts_resources = gnome.compile_resources(
  'cc-global-shortcuts-provider-resources',
  'global-shortcuts-provider.gresource.xml',
  c_name: 'cc_global_shortcut_provider',
  export: true,
  dependencies: blueprints,
)

sources += global_shortcuts_resources

sources += gnome.gdbus_codegen(
  'cc-global-shortcuts-provider-generated',
  'org.gnome.GlobalShortcutsProvider.xml',
  interface_prefix: 'org.gnome.',
  namespace: 'Cc'
)

executable(
  'gnome-control-center-global-shortcuts-provider',
  sources,
  include_directories: [top_inc],
  dependencies:  [
    shell_deps,
    keyboard_panel_dep,
    dependency('libgxdp'),
  ],
  c_args: cflags,
  install: true,
  install_dir: control_center_libexecdir
)
