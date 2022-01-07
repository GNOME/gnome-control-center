gcr_dep = [dependency('gcr-3')]

deps = common_deps + network_manager_deps + gcr_dep + [polkit_gobject_dep]
panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

desktop_in = configure_file(
          input : desktop + '.in.in',
         output : desktop + '.in',
  configuration : desktop_conf
)

i18n.merge_file(
         type : 'desktop',
        input : desktop_in,
       output : desktop,
       po_dir : po_dir,
      install : true,
  install_dir : control_center_desktopdir
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

resource_data = files(
  'cc-wwan-panel.ui',
  'cc-wwan-device-page.ui',
  'cc-wwan-mode-dialog.ui',
  'cc-wwan-network-dialog.ui',
  'cc-wwan-details-dialog.ui',
  'cc-wwan-sim-lock-dialog.ui',
  'cc-wwan-apn-dialog.ui',
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
        c_name : 'cc_' + cappletname,
  dependencies : resource_data,
        export : true
)

cflags += '-DGNOMELOCALEDIR="@0@"'.format(control_center_localedir)

panels_libs += static_library(
           cappletname,
              sources : sources,
  include_directories : [ top_inc, common_inc ],
         dependencies : deps,
               c_args : cflags
)
