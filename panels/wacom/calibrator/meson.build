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

calibrator_deps = deps + [libdevice_dep, m_dep]

libwacom_calibrator = static_library(
  cappletname + '-calibrator',
  sources: common_sources,
  include_directories: top_inc,
  dependencies: calibrator_deps,
  c_args: cflags
)

libwacom_calibrator_test = static_library(
  cappletname + '-calibrator-test',
  sources: common_sources,
  include_directories: top_inc,
  dependencies: calibrator_deps,
  c_args: test_cflags
)
