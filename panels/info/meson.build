panel_names = [
  'default-apps',
  'info-overview',
  'removable-media'
]

panels_list += panel_names

foreach name: panel_names
  desktop = 'gnome-' + name + '-panel.desktop'

  desktop_in = configure_file(
    input: desktop + '.in.in',
    output: desktop + '.in',
    configuration: desktop_conf
  )

  i18n.merge_file(
    desktop,
    type: 'desktop',
    input: desktop_in,
    output: desktop,
    po_dir: po_dir,
    install: true,
    install_dir: control_center_desktopdir
  )
endforeach

cflags += [
  '-DBINDIR="@0@"'.format(control_center_bindir),
  '-DDATADIR="@0@"'.format(control_center_datadir),
  '-DGNOME_SESSION_DIR="@0@"'.format(gnome_session_libexecdir),
  '-DGNOMELOCALEDIR="@0@"'.format(control_center_localedir)
]

gsd_headers = ['gsd-disk-space-helper.h']

gsd_sources = ['gsd-disk-space-helper.c']

sources = files(gsd_sources) + files(
  'cc-info-default-apps-panel.c',
  'cc-info-overview-panel.c',
  'cc-info-removable-media-panel.c',
  'info-cleanup.c'
)

resource_data = files(
  'GnomeLogoVerticalMedium.svg',
  'info-default-apps.ui',
  'info-overview.ui',
  'info-removable-media.ui'
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname,
  dependencies: resource_data,
  export: true
)

deps = common_deps + [
  polkit_gobject_dep,
  dependency('libgtop-2.0')
]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags
)

test_name = 'test-info-cleanup'

sources = files(
  'info-cleanup.c',
  test_name + '.c'
)

cflags += ['-DTEST_SRCDIR="@0@"'.format(meson.current_source_dir())]

exe = executable(
  test_name,
  sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags
)

test(name, exe)

# FIXME: workaround for updating different sources code
input_dir = join_paths(meson.source_root(), '..', 'gnome-settings-daemon', 'plugins', 'housekeeping')

script_conf = configuration_data()
script_conf.set('program', update_from_gsd.path())
script_conf.set('input_dir', input_dir)
script_conf.set('working_dir', meson.current_source_dir())
script_conf.set('source_files', ' '.join(gsd_sources + gsd_headers))
script_conf.set('source_message', 'info: Update from gnome-settings-daemon')

script_name = 'update-info-from-gsd'

script = configure_file(
  input: update_from_gsd_in,
  output: script_name + '.sh',
  configuration: script_conf
)

run_target(
  script_name,
  command: script
)
