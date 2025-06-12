subdir('applications')

###########
# Sources #
###########

ui_sources = files(
  'gtp-dynamic-panel.blp',
  'gtp-static-init.blp',
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
