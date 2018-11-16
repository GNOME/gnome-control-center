subdir('applications')

###########
# Sources #
###########

sources = files(
  'gtp-dynamic-panel.c',
  'gtp-header-widget.c',
  'gtp-sidebar-widget.c',
  'gtp-static-init.c',
  'main.c',
)


##############
# GResources #
##############

resource_data = files(
  'gtp-dynamic-panel.ui',
  'gtp-header-widget.ui',
  'gtp-sidebar-widget.ui',
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
