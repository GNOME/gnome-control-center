panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

i18n.merge_file(
  type: 'desktop',
  input: desktop + '.in',
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: control_center_desktopdir
)

sources = files(
  'cc-system-panel.c',
  'region/cc-region-page.c',
  'region/cc-format-chooser.c',
  'region/cc-format-preview.c',
  'remote-desktop/cc-gnome-remote-desktop.c',
  'remote-desktop/cc-remote-desktop-page.c',
  'remote-desktop/cc-tls-certificate.c',
  'remote-desktop/cc-systemd-service.c',
)

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name : 'cc_' + cappletname,
  export : true
)

libsecret_dep = dependency('libsecret-1')
gnutls_dep = dependency('gnutls')

deps = common_deps + [
  accounts_dep,
  gcr_dep,
  gnome_desktop_dep,
  gnutls_dep,
  liblanguage_dep,
  libsecret_dep,
  polkit_gobject_dep,
  pwquality_dep,
]

if enable_ibus
  deps += ibus_dep
endif

settings_daemon = 'org.gnome.SettingsDaemon'
gdbus = settings_daemon + '.Sharing'

sources += gnome.gdbus_codegen(
  gdbus,
  gdbus + '.xml',
  interface_prefix: settings_daemon + '.',
  namespace: 'Gsd'
)

subdir('region')

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc ],
  dependencies: deps,
  c_args: cflags
)
