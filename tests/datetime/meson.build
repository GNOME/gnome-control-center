
test_units = [
  #'test-timezone',
  'test-timezone-gfx',
  'test-endianess',
]

includes = [top_inc, include_directories('../../panels/datetime')]
cflags = [
  '-DTEST_SRCDIR="@0@"'.format(meson.current_source_dir()),
  '-DSRCDIR="@0@"'.format(meson.source_root() + '/panels/datetime')
]

foreach unit: test_units
  exe = executable(
    unit,
    [unit + '.c'],
    include_directories: includes,
    dependencies: common_deps + [m_dep],
    link_with: [datetime_panel_lib],
    c_args: cflags
  )

  test(unit, exe)
endforeach

