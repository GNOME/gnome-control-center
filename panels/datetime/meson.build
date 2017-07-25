panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

desktop_in = configure_file(
  input: desktop + '.in.in',
  output: desktop + '.in',
  configuration: desktop_conf
)

custom_target(
  desktop,
  input: desktop_in,
  output: desktop,
  command: intltool_desktop_cmd,
  install: true,
  install_dir: control_center_desktopdir
)

polkit = 'org.gnome.controlcenter.@0@.policy'.format(cappletname)

custom_target(
  polkit,
  input: polkit + '.in',
  output: polkit,
  command: intltool_xml_cmd,
  install: true,
  install_dir: join_paths(control_center_datadir, 'polkit-1', 'actions')
)

resource_data = files(
  'data/bg_dim.png',
  'data/bg.png',
  'data/cc.png',
  'data/pin.png',
  'data/timezone_0_dim.png',
  'data/timezone_0.png',
  'data/timezone_10.5_dim.png',
  'data/timezone_10.5.png',
  'data/timezone_-10_dim.png',
  'data/timezone_10_dim.png',
  'data/timezone_-10.png',
  'data/timezone_10.png',
  'data/timezone_11.5_dim.png',
  'data/timezone_11.5.png',
  'data/timezone_-11_dim.png',
  'data/timezone_11_dim.png',
  'data/timezone_-11.png',
  'data/timezone_11.png',
  'data/timezone_12.75_dim.png',
  'data/timezone_12.75.png',
  'data/timezone_12_dim.png',
  'data/timezone_12.png',
  'data/timezone_13_dim.png',
  'data/timezone_13.png',
  'data/timezone_14_dim.png',
  'data/timezone_14.png',
  'data/timezone_-1_dim.png',
  'data/timezone_1_dim.png',
  'data/timezone_-1.png',
  'data/timezone_1.png',
  'data/timezone_-2_dim.png',
  'data/timezone_2_dim.png',
  'data/timezone_-2.png',
  'data/timezone_2.png',
  'data/timezone_-3.5_dim.png',
  'data/timezone_3.5_dim.png',
  'data/timezone_-3.5.png',
  'data/timezone_3.5.png',
  'data/timezone_-3_dim.png',
  'data/timezone_3_dim.png',
  'data/timezone_-3.png',
  'data/timezone_3.png',
  'data/timezone_-4.5_dim.png',
  'data/timezone_4.5_dim.png',
  'data/timezone_-4.5.png',
  'data/timezone_4.5.png',
  'data/timezone_-4_dim.png',
  'data/timezone_4_dim.png',
  'data/timezone_-4.png',
  'data/timezone_4.png',
  'data/timezone_-5.5_dim.png',
  'data/timezone_5.5_dim.png',
  'data/timezone_-5.5.png',
  'data/timezone_5.5.png',
  'data/timezone_5.75_dim.png',
  'data/timezone_5.75.png',
  'data/timezone_-5_dim.png',
  'data/timezone_5_dim.png',
  'data/timezone_-5.png',
  'data/timezone_5.png',
  'data/timezone_6.5_dim.png',
  'data/timezone_6.5.png',
  'data/timezone_-6_dim.png',
  'data/timezone_6_dim.png',
  'data/timezone_-6.png',
  'data/timezone_6.png',
  'data/timezone_-7_dim.png',
  'data/timezone_7_dim.png',
  'data/timezone_-7.png',
  'data/timezone_7.png',
  'data/timezone_8.5_dim.png',
  'data/timezone_8.5.png',
  'data/timezone_8.75_dim.png',
  'data/timezone_8.75.png',
  'data/timezone_-8_dim.png',
  'data/timezone_8_dim.png',
  'data/timezone_-8.png',
  'data/timezone_8.png',
  'data/timezone_-9.5_dim.png',
  'data/timezone_9.5_dim.png',
  'data/timezone_-9.5.png',
  'data/timezone_9.5.png',
  'data/timezone_-9_dim.png',
  'data/timezone_9_dim.png',
  'data/timezone_-9.png',
  'data/timezone_9.png',
  'backward',
  'big.ui',
  'datetime.ui',
  'little.ui',
  'middle.ui',
  'ydm.ui'
)

resources = gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  dependencies: resource_data,
  export: true
)

sources = files(
  'cc-datetime-panel.c',
  'cc-timezone-map.c',
  'date-endian.c',
  'tz.c'
)

gdbus = 'timedated'
gdbus_iface_xml = gdbus + '1-interface.xml'

# This requires running d-bus session and accessible timedate1 daemon
# FIXME: need to find a way how to filter out unnecessary d-bus stuff (introspectable, properties)
#timedated1-interface.xml:
# gdbus introspect          \
#   --xml           \
#   --system          \
#   --dest org.freedesktop.timedate1    \
#   --object-path /org/freedesktop/timedate1  \
#    > timedated1-interface.xml
'''
gdbus_iface = custom_target(
  gdbus_iface_xml,
  output: gdbus_iface_xml,
  capture: true,
  command: [
    find_program('gdbus'),
    'introspect',
    '--xml',
    '--system',
    '--dest', 'org.freedesktop.timedate1',
    '--object-path', '/org/freedesktop/timedate1'
  ],
)
'''

sources += gnome.gdbus_codegen(
  gdbus,
  gdbus_iface_xml,
  interface_prefix: 'org.freedesktop.'
)

deps = common_deps + [
  gdk_pixbuf_dep,
  gnome_desktop_dep,
  polkit_gobject_dep
]

cflags += [
  '-DGNOMECC_DATA_DIR="@0@"'.format(control_center_pkgdatadir),
  '-DGNOMELOCALEDIR="@0@"'.format(control_center_localedir)
]

panels_libs += static_library(
  cappletname,
  sources: sources + resources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags
)

test_cflags = '-DSRCDIR="@0@"'.format(meson.current_source_dir())

test_units = [
  #['test-timezone', ['cc-timezone-map.c', 'tz.c'] + resources, [m_dep], []],
  ['test-timezone-gfx', ['tz.c'] + resources, [m_dep], [test_cflags]],
  ['test-endianess', ['date-endian.c'], [], []]
]

foreach unit: test_units
  exe = executable(
    unit[0],
    [unit[0] + '.c'] + unit[1],
    include_directories: top_inc,
    dependencies: deps + unit[2],
    c_args: cflags + unit[3]
  )

  test(unit[0], exe)
endforeach

subdir('po-timezones')
subdir('icons')
