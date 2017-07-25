icon_sizes = [
  '16x16',
  '22x22',
  '24x24',
  '32x32',
  '48x48',
  '256x256'
]

foreach icon_size: icon_sizes
  install_data(
    join_paths(icon_size, 'goa-panel.png'),
    install_dir: join_paths(control_center_icondir, 'hicolor', icon_size, 'apps')
  )
endforeach
