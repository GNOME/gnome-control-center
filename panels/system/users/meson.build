desktop = 'gnome-users-panel.desktop'
i18n.merge_file(
  type: 'desktop',
  input: desktop + '.in',
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: control_center_desktopdir
)

ui_sources = files(
  'cc-avatar-chooser.blp',
  'cc-enterprise-login-dialog.blp',
  'cc-entry-feedback.blp',
  'cc-fingerprint-dialog.blp',
  'cc-user-page.blp',
  'cc-users-page.blp',
)

ui_compiled = []
foreach ui_source : ui_sources
  ui_compiled += fs.name(fs.replace_suffix(ui_source, '.ui'))
endforeach

blueprints += custom_target(
  input: ui_sources,
  output: ui_compiled,
  command: [blueprint_compiler, 'batch-compile', '@OUTDIR@', '@CURRENT_SOURCE_DIR@', '@INPUT@']
)

image_data = files(
  'data/faces/bicycle.jpg',
  'data/faces/book.jpg',
  'data/faces/calculator.jpg',
  'data/faces/cat.jpg',
  'data/faces/coffee2.jpg',
  'data/faces/flower2.jpg',
  'data/faces/gamepad.jpg',
  'data/faces/guitar2.jpg',
  'data/faces/headphones.jpg',
  'data/faces/hummingbird.jpg',
  'data/faces/mountain.jpg',
  'data/faces/plane.jpg',
  'data/faces/surfer.jpg',
  'data/faces/tomatoes.jpg',
  'data/faces/tree.jpg',
)

image_dir = join_paths(control_center_datadir, 'pixmaps', 'faces')

install_data(
  image_data,
  install_dir: image_dir
)

polkit = 'org.gnome.controlcenter.user-accounts.policy'
i18n.merge_file(
  input: polkit + '.in',
  output: polkit,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(control_center_datadir, 'polkit-1', 'actions')
)

realmd_namespace = 'org.freedesktop.realmd'
sources += gnome.gdbus_codegen(
  'cc-realm-generated',
  'data/' + realmd_namespace + '.xml',
  interface_prefix: realmd_namespace + '.',
  namespace: 'CcRealm',
  object_manager: true,
  annotations: ['org.freedesktop.realmd.Realm', 'org.gtk.GDBus.C.Name', 'Common']
)

sources += gnome.mkenums_simple(
  'cc-user-accounts-enum-types',
  sources: ['cc-fingerprint-manager.h']
)

fprintd_namespace = 'net.reactivated.Fprint'
sources += gnome.gdbus_codegen(
  'cc-fprintd-generated',
  sources: [
    'data' / fprintd_namespace + '.Manager.xml',
    'data' / fprintd_namespace + '.Device.xml',
  ],
  interface_prefix: fprintd_namespace + '.',
  namespace: 'CcFprintd',
  autocleanup: 'all',
)

# Kerberos support
krb_dep = dependency('krb5', required: false)
assert(krb_dep.found(), 'kerberos libraries not found in your path')

cflags += [
  '-DGNOMELOCALEDIR="@0@"'.format(control_center_localedir),
  '-DHAVE_LIBPWQUALITY',
  '-DUM_PIXMAP_DIR="@0@"'.format(join_paths(control_center_pkgdatadir, 'pixmaps'))
]
