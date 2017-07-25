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

sources = files(
  'cc-printers-panel.c',
  'pp-cups.c',
  'pp-details-dialog.c',
  'pp-host.c',
  'pp-ipp-option-widget.c',
  'pp-job.c',
  'pp-jobs-dialog.c',
  'pp-maintenance-command.c',
  'pp-new-printer-dialog.c',
  'pp-new-printer.c',
  'pp-options-dialog.c',
  'pp-ppd-option-widget.c',
  'pp-ppd-selection-dialog.c',
  'pp-print-device.c',
  'pp-printer-entry.c',
  'pp-printer.c',
  'pp-samba.c',
  'pp-utils.c'
)

resource_data = files(
  'authentication-dialog.ui',
  'details-dialog.ui',
  'jobs-dialog.ui',
  'new-printer-dialog.ui',
  'options-dialog.ui',
  'ppd-selection-dialog.ui',
  'printer-entry.ui',
  'printers.ui'
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  dependencies: resource_data,
  export: true
)

deps = common_deps + [
  cups_dep,
  m_dep,
  polkit_gobject_dep,
  dependency('smbclient')
]

cflags += '-DGNOMELOCALEDIR="@0@"'.format(control_center_localedir)

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [top_inc, common_inc, shell_inc],
  dependencies: deps,
  c_args: cflags
)

test_units = [
  'test-canonicalization',
  'test-shift'
]

sources = files(
  'pp-print-device.c',
  'pp-utils.c'
)

cflags += '-DTEST_SRCDIR="@0@"'.format(meson.current_source_dir())

foreach unit: test_units
  exe = executable(
    unit,
    [unit + '.c'] + sources,
    include_directories: top_inc,
    dependencies: deps,
    c_args: cflags
  )

  test(unit, exe)
endforeach
