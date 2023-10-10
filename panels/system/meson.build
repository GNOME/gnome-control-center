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
  'cc-system-region-language-page.c',
  'cc-format-chooser.c',
  'cc-format-preview.c',
  'cc-system-remote-desktop-page.c',
  'cc-gnome-remote-desktop.c',
  'cc-tls-certificate.c',
  'cc-systemd-service.c',
  'about/cc-about-page.c',
  'about/cc-system-details-window.c',
  'about/cc-info-entry.c',
  'about/info-cleanup.c'
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
  common_deps,
  accounts_dep,
  gnome_desktop_dep,
  dependency('udisks2', version: '>= 2.8.2'),
  dependency('libgtop-2.0'),
  gudev_dep,
  liblanguage_dep,
  polkit_gobject_dep,
  libsecret_dep,
  gnutls_dep,
  gcr_dep,
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

cflags += [
   '-DDATADIR="@0@"'.format(control_center_datadir),
   '-DLIBEXECDIR="@0@"'.format(control_center_libexecdir),
]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc ],
  dependencies: deps,
  c_args: cflags
)

subdir('about')
