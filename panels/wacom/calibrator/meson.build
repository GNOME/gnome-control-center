calibrator_inc = include_directories('.')

common_sources = files(
  'calibrator.c',
  'calibrator-gui.c',
  'cc-clock.c',
)

ui_sources = files(
  'calibrator.blp',
)

ui_compiled = []
foreach ui_source : ui_sources
  ui_compiled += fs.name(fs.replace_suffix(ui_source, '.ui'))
endforeach

blueprints += custom_target(
  input: ui_sources,
  output: ui_compiled,
  command: [blueprint_compiler, 'batch-compile', '@OUTDIR@', '@CURRENT_SOURCE_DIR@', '@INPUT@']
)

calibrator_name = cappletname + '-calibrator'
calibrator_test_name = calibrator_name + '-test'

calibrator_deps = deps + [libdevice_dep, m_dep]

libwacom_calibrator = static_library(
  calibrator_name,
  sources: common_sources,
  include_directories: top_inc,
  dependencies: calibrator_deps,
  c_args: cflags
)

libwacom_calibrator_test = static_library(
  calibrator_test_name,
  sources: common_sources,
  include_directories: top_inc,
  dependencies: calibrator_deps,
  c_args: test_cflags
)

wacom_calibrator_test_gresource = gnome.compile_resources(
  'cc-' + calibrator_test_name + '-resources',
  calibrator_test_name + '.gresource.xml',
    source_dir : '.',
        c_name : 'cc_' + calibrator_test_name.underscorify(),
        export : true,
  dependencies : blueprints,
)

sources = common_sources + wacom_calibrator_test_gresource + files('main.c')

executable(
  'test-calibrator',
  sources,
  include_directories: top_inc,
  dependencies: calibrator_deps,
  c_args: cflags
)
