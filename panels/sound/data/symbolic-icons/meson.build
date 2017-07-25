status_files = [
  'audio-input-microphone-high-symbolic.svg',
  'audio-input-microphone-low-symbolic.svg',
  'audio-input-microphone-medium-symbolic.svg',
  'audio-input-microphone-muted-symbolic.svg'
]

foreach file: status_files
  install_data(
    join_paths('scalable', 'status', file),
    install_dir: join_paths(control_center_icondir, 'hicolor', 'scalable', 'status')
  )
endforeach
