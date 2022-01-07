appdata = 'gnome-control-center.appdata.xml'

i18n.merge_file(
  input: appdata + '.in',
  output: appdata,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(control_center_datadir, 'metainfo')
)
