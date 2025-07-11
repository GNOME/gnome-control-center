metainfo_conf = configuration_data()
metainfo_conf.set('appid', application_id)

metainfo_file = i18n.merge_file(
  input: configure_file(
    input: 'org.gnome.Settings.metainfo.xml.in.in',
    output: 'org.gnome.Settings.metainfo.xml.in',
    configuration: metainfo_conf
  ),
  output: '@0@.metainfo.xml'.format(application_id),
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(control_center_datadir, 'metainfo')
)

# Validate AppStream metadata
appstreamcli = find_program('appstreamcli', required: false)
if appstreamcli.found()
  test(
    'validate-metainfo',
    appstreamcli,
    args: ['validate', '--no-net', '--explain', metainfo_file.full_path()],
    depends: metainfo_file
  )
endif
