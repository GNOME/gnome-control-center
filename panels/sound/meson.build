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
  'cc-output-test-dialog.c',
  'cc-profile-combo-box.c',
  'cc-sound-button.c',
  'cc-sound-panel.c',
  'cc-speaker-test-button.c',
  'cc-stream-list-box.c',
  'cc-stream-row.c',
  'cc-subwoofer-slider.c',
  'cc-volume-slider.c',
)

resource_data = files(
  'icons/audio-speaker-center-back.svg',
  'icons/audio-speaker-center-back-testing.svg',
  'icons/audio-speaker-center.svg',
  'icons/audio-speaker-center-testing.svg',
  'icons/audio-speaker-left-back.svg',
  'icons/audio-speaker-left-back-testing.svg',
  'icons/audio-speaker-left-side.svg',
  'icons/audio-speaker-left-side-testing.svg',
  'icons/audio-speaker-left.svg',
  'icons/audio-speaker-left-testing.svg',
  'icons/audio-speaker-mono.svg',
  'icons/audio-speaker-mono-testing.svg',
  'icons/audio-speaker-right-back.svg',
  'icons/audio-speaker-right-back-testing.svg',
  'icons/audio-speaker-right-side.svg',
  'icons/audio-speaker-right-side-testing.svg',
  'icons/audio-speaker-right.svg',
  'icons/audio-speaker-right-testing.svg',
  'icons/audio-speaker-testing.svg',
  'cc-alert-chooser.ui',
  'cc-balance-slider.ui',
  'cc-device-combo-box.ui',
  'cc-fade-slider.ui',
  'cc-output-test-dialog.ui',
  'cc-profile-combo-box.ui',
  'cc-sound-button.ui',
  'cc-sound-panel.ui',
  'cc-speaker-test-button.ui',
  'cc-stream-row.ui',
  'cc-subwoofer-slider.ui',
  'cc-volume-slider.ui',
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
  'sounds/bark.ogg',
  'sounds/drip.ogg',
  'sounds/glass.ogg',
  'sounds/sonar.ogg'
)

install_data(
  sound_data,
  install_dir: join_paths(control_center_datadir, 'sounds', 'gnome', 'default', 'alerts')
)
