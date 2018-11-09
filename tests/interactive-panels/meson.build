#################
# Desktop files #
#################

desktop_files = [
  'dynamic-panel',
  'header-widget',
  'static-init',
]


foreach desktop_file : desktop_files
  i18n.merge_file(
         desktop,
           type : 'desktop',
          input : 'gtp-@0@.desktop.in'.format(desktop_file),
         output : 'gnome-@0@-panel.desktop'.format(desktop_file),
         po_dir : po_dir,
        install : true,
    install_dir : control_center_desktopdir
  )
endforeach


###########
# Sources #
###########

sources = files(
  'gtp-dynamic-panel.c',
  'gtp-header-widget.c',
  'gtp-static-init.c',
  'main.c',
)


##############
# GResources #
##############

resource_data = files(
  'gtp-dynamic-panel.ui',
  'gtp-header-widget.ui',
  'gtp-static-init.ui',
)

sources += gnome.compile_resources(
  'test-panels-resources',
   'panels.gresource.xml',
              source_dir : '.',
                  c_name : 'test_panels',
            dependencies : resource_data,
                  export : true,
)


######################
# interactive-panels #
######################

includes = [top_inc]

exe = executable(
  'test-interactive-panels',
               sources,
  include_directories : includes,
         dependencies : shell_deps + [libtestshell_dep],
               c_args : cflags
)
