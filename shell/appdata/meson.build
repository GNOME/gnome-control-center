appdata = 'gnome-control-center.appdata.xml'

custom_target(
  appdata,
  input: appdata + '.in',
  output: appdata,
  command: intltool_xml_cmd,
  install: true,
  install_dir: join_paths(control_center_datadir, 'metainfo')
)
