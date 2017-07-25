apps_files = [
  'multimedia-volume-control.png',
  'multimedia-volume-control.svg'
]

device_files = ['audio-headset.svg']

status_files = [
  'audio-input-microphone-high.png',
  'audio-input-microphone-low.png',
  'audio-input-microphone-medium.png',
  'audio-input-microphone-muted.png'
]

scalable_device_files = [
  'audio-speaker-center-back.svg',
  'audio-speaker-center-back-testing.svg',
  'audio-speaker-center.svg',
  'audio-speaker-center-testing.svg',
  'audio-speaker-left-back.svg',
  'audio-speaker-left-back-testing.svg',
  'audio-speaker-left-side.svg',
  'audio-speaker-left-side-testing.svg',
  'audio-speaker-left.svg',
  'audio-speaker-left-testing.svg',
  'audio-speaker-mono.svg',
  'audio-speaker-mono-testing.svg',
  'audio-speaker-right-back.svg',
  'audio-speaker-right-back-testing.svg',
  'audio-speaker-right-side.svg',
  'audio-speaker-right-side-testing.svg',
  'audio-speaker-right.svg',
  'audio-speaker-right-testing.svg',
  'audio-speaker-testing.svg',
  'audio-subwoofer.svg',
  'audio-subwoofer-testing.svg'
]

icon_sizes = [
  ['16x16', [apps_files, device_files, status_files]],
  ['22x22', [apps_files, [], status_files]],
  ['24x24', [['multimedia-volume-control.png'], device_files, status_files]],
  ['32x32', [apps_files, device_files, status_files]],
  ['48x48', [['multimedia-volume-control.png'], device_files, []]],
  ['scalable', [['multimedia-volume-control.svg'], scalable_device_files, []]]
]

foreach icon_size: icon_sizes
  foreach file: icon_size[1][0]
    install_data(
      join_paths(icon_size[0], 'apps', file),
      install_dir: join_paths(control_center_icondir, 'hicolor', icon_size[0], 'apps')
    )
  endforeach

  foreach file: icon_size[1][1]
    install_data(
      join_paths(icon_size[0], 'devices', file),
      install_dir: join_paths(control_center_pkgdatadir, 'icons', 'hicolor', icon_size[0], 'devices')
    )
  endforeach

  foreach file: icon_size[1][2]
    install_data(
      join_paths(icon_size[0], 'status', file),
      install_dir: join_paths(control_center_pkgdatadir, 'icons', 'hicolor', icon_size[0], 'status')
    )
  endforeach
endforeach
