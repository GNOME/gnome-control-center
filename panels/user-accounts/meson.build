panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

desktop_in = configure_file(
  input: 'data/' + desktop + '.in.in',
  output: desktop + '.in',
  configuration: desktop_conf
)

custom_target(
  desktop,
  input: desktop_in,
  output: desktop,
  command: intltool_desktop_cmd,
  install: true,
  install_dir: control_center_desktopdir
)

image_data = files(
  'data/faces/astronaut.jpg',
  'data/faces/baseball.png',
  'data/faces/butterfly.png',
  'data/faces/cat-eye.jpg',
  'data/faces/chess.jpg',
  'data/faces/coffee.jpg',
  'data/faces/dice.jpg',
  'data/faces/energy-arc.jpg',
  'data/faces/fish.jpg',
  'data/faces/flake.jpg',
  'data/faces/flower.jpg',
  'data/faces/grapes.jpg',
  'data/faces/guitar.jpg',
  'data/faces/launch.jpg',
  'data/faces/leaf.jpg',
  'data/faces/lightning.jpg',
  'data/faces/penguin.jpg',
  'data/faces/puppy.jpg',
  'data/faces/sky.jpg',
  'data/faces/soccerball.png',
  'data/faces/sunflower.jpg',
  'data/faces/sunset.jpg',
  'data/faces/tennis-ball.png',
  'data/faces/yellow-rose.jpg'
)

install_data(
  image_data,
  install_dir: join_paths(control_center_datadir, 'pixmaps', 'faces')
)

polkit = 'org.gnome.controlcenter.@0@.policy'.format(cappletname)

custom_target(
  polkit,
  input: polkit + '.in',
  output: polkit,
  command: intltool_xml_cmd,
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
  'um-cell-renderer-user-image.c',
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
  libgd_dep,
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
