panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

desktop_in = configure_file(
  input: desktop + '.in.in',
  output: desktop + '.in',
  configuration: desktop_conf
)

i18n.merge_file(
  type: 'desktop',
  input: desktop_in,
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: control_center_desktopdir
)

polkit = 'org.gnome.controlcenter.@0@.policy'.format(cappletname)

i18n.merge_file(
  input: polkit + '.in',
  output: polkit,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(control_center_datadir, 'polkit-1', 'actions')
)

resource_data = files(
  'data/bg_dim.png',
  'data/bg.png',
  'data/pin.png',
  'backward',
  'cc-datetime-panel.ui',
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
  '-DGNOMECC_DATA_DIR="@0@"'.format(control_center_pkgdatadir)
]

datetime_panel_lib = static_library(
  cappletname,
  sources: sources + resources,
  include_directories: [ top_inc, common_inc ],
  dependencies: deps,
  c_args: cflags
)
panels_libs += datetime_panel_lib

datetime_panel_lib_dep = declare_dependency(
              sources : resources,
  include_directories : [top_inc, include_directories('.')],
            link_with : datetime_panel_lib
)

subdir('po-timezones')
subdir('icons')
