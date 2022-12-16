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

deps = common_deps + [
  libgvc_dep,
  libxml_dep,
  m_dep,
  pulse_dep,
  pulse_mainloop_dep,
  dependency('gsound'),
]

cflags += [
  '-DSOUND_DATA_DIR="@0@"'.format(join_paths(control_center_datadir, 'sounds')),
]

sources = files(
  'cc-alert-chooser.c',
  'cc-balance-slider.c',
  'cc-device-combo-box.c',
  'cc-fade-slider.c',
  'cc-level-bar.c',
  'cc-output-test-wheel.c',
  'cc-output-test-window.c',
  'cc-profile-combo-box.c',
  'cc-sound-panel.c',
  'cc-speaker-test-button.c',
  'cc-stream-list-box.c',
  'cc-stream-row.c',
  'cc-subwoofer-slider.c',
  'cc-volume-slider.c',
)

resource_data = files(
  'icons/audio-speaker-center-back.svg',
  'icons/audio-speaker-center.svg',
  'icons/audio-speaker-left-back.svg',
  'icons/audio-speaker-left-side.svg',
  'icons/audio-speaker-left.svg',
  'icons/audio-speaker-right-back.svg',
  'icons/audio-speaker-right-side.svg',
  'icons/audio-speaker-right.svg',
  'cc-alert-chooser.ui',
  'cc-balance-slider.ui',
  'cc-device-combo-box.ui',
  'cc-fade-slider.ui',
  'cc-output-test-window.ui',
  'cc-profile-combo-box.ui',
  'cc-sound-panel.ui',
  'cc-stream-row.ui',
  'cc-subwoofer-slider.ui',
  'cc-volume-slider.ui',
  'output-test-wheel.css',
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname.underscorify (),
  dependencies: resource_data,
  export: true
)

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [top_inc, common_inc],
  dependencies: deps,
  c_args: cflags,
)

sound_data = files(
  'sounds/click.ogg',
  'sounds/hum.ogg',
  'sounds/string.ogg',
  'sounds/swing.ogg'
)

install_data(
  sound_data,
  install_dir: join_paths(control_center_datadir, 'sounds', 'gnome', 'default', 'alerts')
)

install_data(
  'icons/org.gnome.Settings-sound-symbolic.svg',
  install_dir: join_paths(control_center_icondir, 'hicolor', 'scalable', 'apps')
)
