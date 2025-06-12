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

polkit = 'org.gnome.controlcenter.user-accounts.policy'.format(cappletname)
i18n.merge_file(
  input: 'users/' + polkit + '.in',
  output: polkit,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(control_center_datadir, 'polkit-1', 'actions')
)

ui_sources = files(
  'cc-system-panel.blp',
)

ui_compiled = []
foreach ui_source : ui_sources
  ui_compiled += fs.name(fs.replace_suffix(ui_source, '.ui'))
endforeach

blueprints = []

blueprints += custom_target(
  input: ui_sources,
  output: ui_compiled,
  command: [blueprint_compiler, 'batch-compile', '@OUTDIR@', '@CURRENT_SOURCE_DIR@', '@INPUT@']
)

sources = files(
  'cc-password-utils.c',
  'cc-system-panel.c',
  'cc-systemd-service.c',
  'about/cc-about-page.c',
  'about/cc-system-details-window.c',
  'about/cc-info-entry.c',
  'about/info-cleanup.c',
  'datetime/cc-datetime-page.c',
  'datetime/cc-tz-item.c',
  'datetime/cc-tz-dialog.c',
  'datetime/date-endian.c',
  'datetime/tz.c',
  'region/cc-region-page.c',
  'region/cc-format-chooser.c',
  'region/cc-format-preview.c',
  'remote-desktop/cc-desktop-sharing-page.c',
  'remote-desktop/cc-encryption-fingerprint-dialog.c',
  'remote-desktop/cc-gnome-remote-desktop.c',
  'remote-desktop/cc-remote-desktop-page.c',
  'remote-desktop/cc-remote-login-page.c',
  'remote-desktop/cc-tls-certificate.c',
  'secure-shell/cc-secure-shell.c',
  'secure-shell/cc-secure-shell-page.c',
  'users/cc-add-user-dialog.c',
  'users/cc-avatar-chooser.c',
  'users/cc-crop-area.c',
  'users/cc-entry-feedback.c',
  'users/cc-enterprise-login-dialog.c',
  'users/cc-fingerprint-dialog.c',
  'users/cc-fingerprint-manager.c',
  'users/cc-password-dialog.c',
  'users/cc-realm-manager.c',
  'users/cc-user-page.c',
  'users/cc-users-page.c',
  'users/pw-utils.c',
  'users/run-passwd.c',
  'users/user-utils.c',
)

gdesktop_enums_header = files(
  gsettings_desktop_dep.get_variable(pkgconfig: 'prefix') + '/include/gsettings-desktop-schemas/gdesktop-enums.h'
)

sources += gnome.mkenums_simple(
  'gdesktop-enums-types',
  sources: gdesktop_enums_header,
  identifier_prefix: 'GDesktop',
  symbol_prefix: 'g_desktop'
)

libsecret_dep = dependency('libsecret-1')
gnutls_dep = dependency('gnutls')
libgtop2 = dependency('libgtop-2.0')
udisks2 = dependency('udisks2', version: '>= 2.8.2')

# Kerberos support
krb_dep = dependency('krb5', required: false)
assert(krb_dep.found(), 'kerberos libraries not found in your path')

deps = common_deps + [
  accounts_dep,
  gcr_dep,
  gdk_pixbuf_dep,
  gnome_desktop_dep,
  gnutls_dep,
  gudev_dep,
  libgtop2,
  liblanguage_dep,
  libsecret_dep,
  krb_dep,
  polkit_gobject_dep,
  pwquality_dep,
  udisks2,
]

if enable_ibus
  deps += ibus_dep
endif

if enable_malcontent
  deps += malcontent_dep
endif

settings_daemon = 'org.gnome.SettingsDaemon'
gsd_gdbus = settings_daemon + '.Sharing'

sources += gnome.gdbus_codegen(
  gsd_gdbus,
  gsd_gdbus + '.xml',
  interface_prefix: settings_daemon + '.',
  namespace: 'Gsd'
)

cflags += [
   '-DDATADIR="@0@"'.format(control_center_datadir),
   '-DSYSCONFDIR="@0@"'.format(control_center_sysconfdir),
   '-DLIBEXECDIR="@0@"'.format(control_center_libexecdir),
  '-DGNOMECC_DATA_DIR="@0@"'.format(control_center_pkgdatadir)
]

subdir('about')
subdir('datetime')
subdir('icons')
subdir('region')
subdir('remote-desktop')
subdir('secure-shell')
subdir('users')

sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name : 'cc_' + cappletname,
  export : true,
  dependencies : blueprints,
)

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [ top_inc, common_inc, include_directories('.'), include_directories('users')],
  dependencies: deps,
  c_args: cflags
)
