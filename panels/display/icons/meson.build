icon_sizes = [
  '16x16',
  '22x22',
  '24x24',
  '32x32'
]

foreach icon_size: icon_sizes
  install_data(
    join_paths(icon_size, 'preferences-desktop-display.png'),
    install_dir: join_paths(control_center_icondir, 'hicolor', icon_size, 'apps')
  )
endforeach

install_data(
  'scalable/preferences-desktop-display.svg',
  install_dir: join_paths(control_center_icondir, 'hicolor', 'scalable', 'apps')
)
