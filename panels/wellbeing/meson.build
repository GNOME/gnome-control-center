# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2024 GNOME Foundation, Inc.

panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

i18n.merge_file(
  type: 'desktop',
  input: desktop + '.in',
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: control_center_desktopdir,
)

ui_sources = files(
  'cc-bar-chart-bar.blp',
  'cc-bar-chart-group.blp',
  'cc-bar-chart.blp',
  'cc-break-schedule-row.blp',
  'cc-screen-time-statistics-row.blp',
  'cc-wellbeing-panel.blp',
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
  'cc-bar-chart.c',
  'cc-bar-chart-bar.c',
  'cc-bar-chart-group.c',
  'cc-break-schedule-row.c',
  'cc-screen-time-statistics-row.c',
  'cc-wellbeing-panel.c',
)

sources += gnome.gdbus_codegen(
  'cc-shell-wellbeing-generated',
  'data/org.gnome.Shell.Wellbeing.xml',
  interface_prefix: 'org.gnome.Shell.Wellbeing.',
  namespace: 'CcShellWellbeing',
  object_manager: true,
  autocleanup: 'all',
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname.underscorify(),
  export: true,
  dependencies: blueprints,
)

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc ],
  dependencies: common_deps + [json_glib_dep],
  c_args: cflags,
)

subdir('icons')
