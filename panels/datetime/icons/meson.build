icon_sizes = [
  '16x16',
  '22x22',
  '32x32',
  '48x48',
  '256x256'
]

foreach icon_size: icon_sizes
  install_data(
    join_paths(icon_size, 'preferences-system-time.png'),
    install_dir: join_paths(control_center_icondir, 'hicolor', icon_size, 'apps')
  )
endforeach

install_data(
  'scalable/preferences-system-time.svg',
  install_dir: join_paths(control_center_icondir, 'hicolor', 'scalable', 'apps')
)
