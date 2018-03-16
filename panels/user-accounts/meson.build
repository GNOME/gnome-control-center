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
                         'for f in $DESTDIR@0@/*; do
                          ln -s legacy/$(basename $f) $DESTDIR@1@/$(basename $f);
                          done'.format(legacy_image_dir, image_dir))

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
  'pw-utils.c',
  'um-account-dialog.c',
  'um-realm-manager.c',
  'um-utils.c'
)

resource_data = files(
  'data/icons/left-index-finger.png',
  'data/icons/left-little-finger.png',
  'data/icons/left-middle-finger.png',
  'data/icons/left-ring-finger.png',
  'data/icons/left-thumb.png',
  'data/icons/print_error.png',
  'data/icons/print_ok.png',
  'data/icons/right-index-finger.png',
  'data/icons/right-little-finger.png',
  'data/icons/right-middle-finger.png',
  'data/icons/right-ring-finger.png',
  'data/icons/right-thumb.png',
  'data/account-dialog.ui',
  'data/account-fingerprint.ui',
  'data/carousel.css',
  'data/carousel.ui',
  'data/history-dialog.ui',
  'data/join-dialog.ui',
  'data/password-dialog.ui',
  'data/user-accounts-dialog.css',
  'data/user-accounts-dialog.ui'
)

common_sources += gnome.compile_resources(
  'um-resources',
  cappletname + '.gresource.xml',
  c_name: 'um',
  dependencies: resource_data,
  export: true
)

realmd_namespace = 'org.freedesktop.realmd'

common_sources += gnome.gdbus_codegen(
  'um-realm-generated',
  'data/' + realmd_namespace + '.xml',
  interface_prefix: realmd_namespace + '.',
  namespace: 'UmRealm',
  object_manager: true,
  annotations: ['org.freedesktop.realmd.Realm', 'org.gtk.GDBus.C.Name', 'Common']
)

sources = common_sources + files(
  'cc-crop-area.c',
  'run-passwd.c',
  'um-account-type.c',
  'um-carousel.c',
  'um-fingerprint-dialog.c',
  'um-history-dialog.c',
  'um-password-dialog.c',
  'um-photo-dialog.c',
  'um-user-image.c',
  'um-user-panel.c'
)

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

name = 'frob-account-dialog'

sources = common_sources + files(name + '.c')

executable(
  name,
  sources,
  include_directories: top_inc,
  dependencies: deps,
  c_args: cflags
)
