panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

desktop_in = configure_file(
  input: desktop + '.in.in',
  output: desktop + '.in',
  configuration: desktop_conf
)

# FIXME: You will need a recent intltool or the patch from this bug
# http://bugzilla.gnome.org/show_bug.cgi?id=462312
custom_target(
  desktop,
  input: desktop_in,
  output: desktop,
  command: intltool_desktop_cmd,
  install: true,
  install_dir: control_center_desktopdir
)

sources = files(
  'cc-display-panel.c',
  'cc-display-config.c',
  'cc-display-config-dbus.c',
  'cc-display-config-manager-dbus.c',
  'cc-display-config-manager.c',
  'cc-night-light-dialog.c',
  'cc-night-light-widget.c',
  'scrollarea.c',
)

resource_data = files(
  'display.ui',
  'icons/16x16/sunset.png',
  'icons/16x16/sunrise.png'
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  source_dir: ['.', 'icons'],
  c_name: 'cc_' + cappletname,
  dependencies: resource_data,
  export: true
)

deps = common_deps + [
  colord_dep,
  gnome_desktop_dep,
  m_dep,
  upower_glib_dep
]

cflags += [
  '-DDATADIR="@0@"'.format(control_center_datadir),
  '-DGNOMELOCALEDIR="@0@"'.format(control_center_localedir)
]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags
)

subdir('icons')
