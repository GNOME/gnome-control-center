desktop = 'gnome-datetime-panel.desktop'
i18n.merge_file(
  type: 'desktop',
  input: desktop + '.in',
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: control_center_desktopdir
)

ui_sources = files(
  'cc-datetime-page.blp',
  'cc-month-row.blp',
  'cc-tz-dialog.blp',
  'cc-tz-row.blp',
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

polkit = 'org.gnome.controlcenter.@0@.policy'.format(cappletname)

i18n.merge_file(
  input: polkit + '.in',
  output: polkit,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(control_center_datadir, 'polkit-1', 'actions')
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

subdir('po-timezones')
