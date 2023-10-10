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
  'about/cc-about-page.c',
  'about/cc-system-details-window.c',
  'about/cc-info-entry.c',
  'about/info-cleanup.c',
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
libgtop2 = dependency('libgtop-2.0')
udisks2 = dependency('udisks2', version: '>= 2.8.2')

deps = common_deps + [
  accounts_dep,
  gcr_dep,
  gnome_desktop_dep,
  gnutls_dep,
  gudev_dep,
  libgtop2,
  liblanguage_dep,
  libsecret_dep,
  polkit_gobject_dep,
  pwquality_dep,
  udisks2,
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

cflags += [
   '-DDATADIR="@0@"'.format(control_center_datadir),
   '-DLIBEXECDIR="@0@"'.format(control_center_libexecdir),
]

subdir('region')

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc ],
  dependencies: deps,
  c_args: cflags
)

subdir('about')
