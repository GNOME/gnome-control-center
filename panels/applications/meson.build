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
  'cc-application-shortcut-dialog.blp',
  'cc-applications-panel.blp',
  'cc-applications-row.blp',
  'cc-default-apps-page.blp',  
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
  'cc-applications-panel.c',
  'cc-applications-row.c',
  'cc-application-shortcut-dialog.c',
  'cc-default-apps-page.c',
  'cc-default-apps-row.c',
  'cc-removable-media-settings.c',
  'globs.c',
  'search.c',
  'utils.c',
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name : 'cc_' + cappletname,
  dependencies: blueprints,
  export : true
)

sources += gnome.gdbus_codegen(
  'cc-global-shortcuts-rebind-generated',
  'org.gnome.GlobalShortcutsRebind.xml',
  interface_prefix : 'org.gnome.',
         namespace : 'Cc'
)

install_data(
  'org.gnome.GlobalShortcutsRebind.xml',
  install_dir : control_center_datadir + '/dbus-1/interfaces',
)

deps = common_deps + [keyboard_shortcuts_dep]

if host_is_linux
  deps += mm_dep
endif

if enable_snap
  deps += [ json_glib_dep, libsoup_dep ]
  sources += [ 'cc-snap-row.c', 'cc-snapd-client.c' ]
endif

if enable_malcontent
  deps += malcontent_dep
endif

panels_libs += static_library(
           cappletname,
              sources : sources,
  include_directories : [ top_inc, common_inc ],
         dependencies : deps,
               c_args : cflags
)

subdir('icons')
