
deps = common_deps + network_manager_deps + [gcr_dep, polkit_gobject_dep]
panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

i18n.merge_file(
         type : 'desktop',
        input : desktop + '.in',
       output : desktop,
       po_dir : po_dir,
      install : true,
  install_dir : control_center_desktopdir
)

ui_sources = files(
  'cc-wwan-apn-dialog.blp',
  'cc-wwan-details-dialog.blp',
  'cc-wwan-device-page.blp',
  'cc-wwan-mode-dialog.blp',
  'cc-wwan-network-dialog.blp',
  'cc-wwan-panel.blp',
  'cc-wwan-sim-lock-dialog.blp',
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
  'cc-wwan-panel.c',
  'cc-wwan-device.c',
  'cc-wwan-data.c',
  'cc-wwan-device-page.c',
  'cc-wwan-mode-dialog.c',
  'cc-wwan-network-dialog.c',
  'cc-wwan-details-dialog.c',
  'cc-wwan-sim-lock-dialog.c',
  'cc-wwan-apn-dialog.c',
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
        c_name : 'cc_' + cappletname,
        export : true,
  dependencies : blueprints,
)

cflags += '-DGNOMELOCALEDIR="@0@"'.format(control_center_localedir)

panels_libs += static_library(
           cappletname,
              sources : sources,
  include_directories : [ top_inc, common_inc ],
         dependencies : deps,
               c_args : cflags
)

subdir('icons')
