sound_data = files(
  'bark.ogg',
  'drip.ogg',
  'glass.ogg',
  'sonar.ogg'
)

install_data(
  sound_data,
  install_dir: join_paths(control_center_datadir, 'sounds', 'gnome', 'default', 'alerts')
)

metadata_conf = configuration_data()
metadata_conf.set('datadir', control_center_datadir)

metadata = 'gnome-sounds-default.xml'

metadata_in = configure_file(
  input: metadata + '.in.in',
  output: metadata + '.in',
  configuration: metadata_conf
)

custom_target(
  metadata,
  input: metadata_in,
  output: metadata,
  command: intltool_xml_cmd,
  install: true,
  install_dir: join_paths(control_center_pkgdatadir, 'sounds')
)
