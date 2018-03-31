

test_unit = 'test-hostname'

sources = files(
  test_unit + '.c'
)

cflags = [
  '-DTEST_SRCDIR="@0@"'.format(meson.current_source_dir()),
  '-DTEST_TOPSRCDIR="@0@"'.format(meson.source_root())
]

exe = executable(
  test_unit,
  sources,
  include_directories: top_inc,
  dependencies: common_panel_deps,
  c_args: cflags
)

test(test_unit, exe)

