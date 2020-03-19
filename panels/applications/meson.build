panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

desktop_in = configure_file(
          input : desktop + '.in.in',
         output : desktop + '.in',
  configuration : desktop_conf
)

i18n.merge_file(
       desktop,
         type : 'desktop',
        input : desktop_in,
       output : desktop,
       po_dir : po_dir,
      install : true,
  install_dir : control_center_desktopdir
)

sources = files(
  'cc-applications-panel.c',
  'cc-applications-row.c',
  'cc-toggle-row.c',
  'cc-info-row.c',
  'cc-action-row.c',
  'globs.c',
  'search.c',
  'utils.c',
)

resource_data = files('cc-applications-panel.ui')

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
        c_name : 'cc_' + cappletname,
  dependencies : resource_data,
        export : true
)

deps = common_deps

if enable_snap
  deps += snapd_glib_deps
  sources += files('cc-snap-row.c')
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
