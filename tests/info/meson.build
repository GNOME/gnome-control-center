
test_units = [
  'test-info-cleanup'
]

includes = [top_inc, include_directories('../../panels/info-overview')]
cflags = '-DTEST_SRCDIR="@0@"'.format(meson.current_source_dir())

foreach unit: test_units
  exe = executable(
                    unit,
           [unit + '.c'],
    include_directories : includes,
           dependencies : common_deps,
              link_with : [info_panel_lib],
                 c_args : cflags
  )

  test(unit, exe)
endforeach

