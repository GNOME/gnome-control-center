panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

desktop_in = configure_file(
  input: 'data/' + desktop + '.in.in',
  output: desktop + '.in',
  configuration: desktop_conf
)

i18n.merge_file(
  desktop,
  type: 'desktop',
  input: desktop_in,
  output: desktop,
  po_dir: po_dir,
  install: true,
  install_dir: control_center_desktopdir
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

legacy_image_data = files(
  'data/faces/legacy/astronaut.jpg',
  'data/faces/legacy/baseball.png',
  'data/faces/legacy/butterfly.png',
  'data/faces/legacy/cat-eye.jpg',
  'data/faces/legacy/chess.jpg',
  'data/faces/legacy/coffee.jpg',
  'data/faces/legacy/dice.jpg',
  'data/faces/legacy/energy-arc.jpg',
  'data/faces/legacy/fish.jpg',
  'data/faces/legacy/flake.jpg',
  'data/faces/legacy/flower.jpg',
  'data/faces/legacy/grapes.jpg',
  'data/faces/legacy/guitar.jpg',
  'data/faces/legacy/launch.jpg',
  'data/faces/legacy/leaf.jpg',
  'data/faces/legacy/lightning.jpg',
  'data/faces/legacy/penguin.jpg',
  'data/faces/legacy/puppy.jpg',
  'data/faces/legacy/sky.jpg',
  'data/faces/legacy/soccerball.png',
  'data/faces/legacy/sunflower.jpg',
  'data/faces/legacy/sunset.jpg',
  'data/faces/legacy/tennis-ball.png',
  'data/faces/legacy/yellow-rose.jpg',
)

image_dir = join_paths(control_center_datadir, 'pixmaps', 'faces')

install_data(
  image_data,
  install_dir: image_dir
)

legacy_image_dir = join_paths(image_dir, 'legacy')

install_data(
  legacy_image_data,
  install_dir: legacy_image_dir
)

# create symlinks for legacy images to not break current images for people
meson.add_install_script('sh', '-c',
                         '''for f in $DESTDIR@0@/*; do
                          ln -sf legacy/$(basename $f) $DESTDIR@1@/$(basename $f);
                          done'''.format(legacy_image_dir, image_dir))

polkit = 'org.gnome.controlcenter.@0@.policy'.format(cappletname)

i18n.merge_file(
  polkit,
  input: polkit + '.in',
  output: polkit,
  po_dir: po_dir,
  install: true,
  install_dir: join_paths(control_center_datadir, 'polkit-1', 'actions')
)

common_sources = files(
  'cc-add-user-dialog.c',
  'cc-realm-manager.c',
  'pw-utils.c',
  'user-utils.c',
)

resource_data = files(
  'cc-add-user-dialog.ui',
  'cc-avatar-chooser.ui',
  'cc-carousel.ui',
  'cc-login-history-dialog.ui',
  'cc-password-dialog.ui',
  'cc-user-panel.ui',
  'cc-fingerprint-dialog.ui',
  'data/icons/fingerprint-detection-complete-symbolic.svg',
  'data/icons/fingerprint-detection-symbolic.svg',
  'data/icons/fingerprint-detection-warning-symbolic.svg',
  'data/carousel.css',
  'data/join-dialog.ui',
  'data/user-accounts-dialog.css',
  'data/cc-fingerprint-dialog.css',
)

common_sources += gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
  c_name: 'cc_' + cappletname.underscorify(),
  dependencies: resource_data,
  export: true
)

realmd_namespace = 'org.freedesktop.realmd'

common_sources += gnome.gdbus_codegen(
  'cc-realm-generated',
  'data/' + realmd_namespace + '.xml',
  interface_prefix: realmd_namespace + '.',
  namespace: 'CcRealm',
  object_manager: true,
  annotations: ['org.freedesktop.realmd.Realm', 'org.gtk.GDBus.C.Name', 'Common']
)

fprintd_namespace = 'net.reactivated.Fprint'
common_sources += gnome.gdbus_codegen(
  'cc-fprintd-generated',
  sources: [
    'data' / fprintd_namespace + '.Manager.xml',
    'data' / fprintd_namespace + '.Device.xml',
  ],
  interface_prefix: fprintd_namespace + '.',
  namespace: 'CcFprintd',
  autocleanup: 'all',
)

enum_headers = [
  'cc-fingerprint-manager.h',
]

sources = common_sources + files(
  'cc-avatar-chooser.c',
  'cc-carousel.c',
  'cc-crop-area.c',
  'cc-fingerprint-manager.c',
  'cc-fingerprint-dialog.c',
  'cc-login-history-dialog.c',
  'cc-password-dialog.c',
  'cc-user-image.c',
  'cc-user-panel.c',
  'run-passwd.c',
)

sources += gnome.mkenums_simple(
  'cc-user-accounts-enum-types',
  sources: files(enum_headers))

# Kerberos support
krb_dep = dependency('krb5', required: false)
assert(krb_dep.found(), 'kerberos libraries not found in your path')

deps = common_deps + [
  accounts_dep,
  gdk_pixbuf_dep,
  gnome_desktop_dep,
  liblanguage_dep,
  krb_dep,
  m_dep,
  polkit_gobject_dep,
  dependency('pwquality', version: '>= 1.2.2')
]

if enable_cheese
  deps += cheese_deps
endif

if enable_malcontent
  deps += malcontent_dep
endif

cflags += [
  '-DGNOMELOCALEDIR="@0@"'.format(control_center_localedir),
  '-DHAVE_LIBPWQUALITY',
  '-DUM_PIXMAP_DIR="@0@"'.format(join_paths(control_center_pkgdatadir, 'pixmaps'))
]

panels_libs += static_library(
  cappletname,
  sources: sources,
  include_directories: [top_inc, shell_inc],
  dependencies: deps,
  c_args: cflags
)
