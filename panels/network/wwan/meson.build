
deps = common_deps + network_manager_deps + [gcr_dep, polkit_gobject_dep]

sources = files(
  'cc-wwan-page.c',
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
  'cc-wwan-resources',
  'wwan.gresource.xml',
  c_name: 'cc_wwan',
  export: true
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
