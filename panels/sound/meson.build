panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

desktop_in = configure_file(
  input: 'data/' + desktop + '.in.in',
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

deps = common_deps + [
  libgvc_dep,
  libxml_dep,
  m_dep,
  pulse_dep,
  pulse_mainloop_dep,
  dependency('libcanberra-gtk3', version: '>= 0.13')
]

cflags += [
  '-DGLADEDIR="@0@"'.format(control_center_pkgdatadir),
  '-DICON_DATA_DIR="@0@"'.format(join_paths(control_center_pkgdatadir, 'icons')),
  '-DLIBEXECDIR="@0@"'.format(control_center_libexecdir),
  '-DLOCALE_DIR="@0@"'.format(control_center_localedir),
  '-DSOUND_DATA_DIR="@0@"'.format(join_paths(control_center_datadir, 'sounds')),
  '-DSOUND_SET_DIR="@0@"'.format(join_paths(control_center_pkgdatadir, 'sounds'))
]

libgvcgtk = static_library(
  'gvcgtk',
  sources: 'gvc-channel-bar.c',
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags
)

sources = files(
  'cc-sound-panel.c',
  'gvc-balance-bar.c',
  'gvc-combo-box.c',
  'gvc-level-bar.c',
  'gvc-mixer-dialog.c',
  'gvc-sound-theme-chooser.c',
  'gvc-speaker-test.c',
  'sound-theme-file-utils.c'
)

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags,
  link_with: libgvcgtk
)

subdir('data/icons')
subdir('data/sounds')
subdir('data/symbolic-icons')
