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
  'cc-printers-panel.blp',
  'pp-details-dialog.blp',
  'pp-job-row.blp',
  'pp-jobs-dialog.blp',
  'pp-options-dialog.blp',
  'pp-ppd-selection-dialog.blp',
  'pp-printer-entry.blp',
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
  'cc-printers-panel.c',
  'pp-cups.c',
  'pp-details-dialog.c',
  'pp-host.c',
  'pp-ipp-option-widget.c',
  'pp-job.c',
  'pp-job-row.c',
  'pp-jobs-dialog.c',
  'pp-maintenance-command.c',
  'pp-new-printer-dialog.c',
  'pp-new-printer.c',
  'pp-options-dialog.c',
  'pp-ppd-option-widget.c',
  'pp-ppd-selection-dialog.c',
  'pp-print-device.c',
  'pp-printer-entry.c',
  'pp-printer.c',
  'pp-samba.c',
  'pp-utils.c'
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  export: true,
  dependencies: blueprints,
)

deps = common_deps + [
  cups_dep,
  m_dep,
  polkit_gobject_dep,
  dependency('smbclient')
]

printers_panel_lib = static_library(
  cappletname,
  sources: sources,
  include_directories: [top_inc, common_inc, shell_inc],
  dependencies: deps,
  c_args: cflags + cups_cflags,
)
panels_libs += [ printers_panel_lib ]

subdir('icons')
