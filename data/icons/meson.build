icon_sizes = [
  '16x16',
  '24x24',
  '32x32',
  '48x48',
  '512x512'
]

foreach icon_size: icon_sizes
  install_data(
    '_'.join(['hicolor', 'apps', icon_size, 'gnome-control-center.png']),
    install_dir: join_paths(control_center_icondir, 'hicolor', icon_size, 'apps')
  )
endforeach

install_data(
  'hicolor_apps_symbolic_gnome-control-center-symbolic.svg',
  install_dir: join_paths(control_center_icondir, 'hicolor', 'symbolic', 'apps')
)
