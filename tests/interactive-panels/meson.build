subdir('applications')

###########
# Sources #
###########

blueprints = custom_target(
  'blueprints',
  input: files(
    'gtp-dynamic-panel.blp',
    'gtp-static-init.blp',
  ),
  output: '.',
  command: [
    blueprint_compiler,
    'batch-compile',
    '@OUTPUT@',
    '@CURRENT_SOURCE_DIR@',
    '@INPUT@',
  ],
)

sources = files(
  'gtp-dynamic-panel.c',
  'gtp-static-init.c',
  'main.c',
)


##############
# GResources #
##############

sources += gnome.compile_resources(
  'test-panels-resources',
   'panels.gresource.xml',
              source_dir : '.',
                  c_name : 'test_panels',
                  export : true,
            dependencies : blueprints,
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
