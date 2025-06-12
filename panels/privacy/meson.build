panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

i18n.merge_file(
  type: 'desktop',
  input: desktop + '.in',
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: control_center_desktopdir
)

blueprints = []

subdir('bolt')
subdir('camera')
subdir('diagnostics')
subdir('firmware-security')
subdir('icons')
subdir('location')
subdir('screen')
subdir('usage')

ui_sources = files(
  'cc-privacy-panel.blp',
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

sources = files(
  'camera/cc-camera-page.c',
  'diagnostics/cc-diagnostics-page.c',
  'firmware-security/cc-firmware-security-boot-dialog.c',
  'firmware-security/cc-firmware-security-dialog.c',
  'firmware-security/cc-firmware-security-help-dialog.c',
  'firmware-security/cc-firmware-security-page.c',
  'firmware-security/cc-firmware-security-utils.c',
  'location/cc-location-page.c',
  'cc-privacy-panel.c',
  'screen/cc-screen-page.c',
  'usage/cc-usage-page.c',
)

if host_is_linux_not_s390
  sources += files(
    'bolt/bolt-client.c',
    'bolt/bolt-device.c',
    'bolt/bolt-enums.c',
    'bolt/bolt-error.c',
    'bolt/bolt-names.c',
    'bolt/bolt-proxy.c',
    'bolt/bolt-str.c',
    'bolt/bolt-time.c',
    'bolt/cc-bolt-device-dialog.c',
    'bolt/cc-bolt-device-entry.c',
    'bolt/cc-bolt-page.c',
  )

  enum_headers = [
    'bolt/bolt-enums.h',
    'bolt/bolt-error.h'
  ]

  sources += gnome.mkenums_simple(
    'bolt-enum-types',
    sources: enum_headers,
  )
endif

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  export: true,
  dependencies: blueprints,
)

cflags += [
  '-DDATADIR="@0@"'.format(control_center_datadir)
]

deps = common_deps + [
  dependency('libgtop-2.0'),
]

deps = deps + [
  polkit_gobject_dep,
  m_dep,
]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc, 'bolt', 'screen', 'usage' ],
  dependencies: deps,
  c_args: cflags
)
