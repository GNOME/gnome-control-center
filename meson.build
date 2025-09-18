project(
  'gnome-control-center', 'c',
        version : '50.alpha',
        license : 'GPL2+',
  meson_version : '>= 0.60.0'
)

control_center_prefix = get_option('prefix')
control_center_bindir = join_paths(control_center_prefix, get_option('bindir'))
control_center_datadir = join_paths(control_center_prefix, get_option('datadir'))
control_center_libexecdir = join_paths(control_center_prefix, get_option('libexecdir'))
control_center_localedir = join_paths(control_center_prefix, get_option('localedir'))
control_center_mandir = join_paths(control_center_prefix, get_option('mandir'))
control_center_sysconfdir = join_paths(control_center_prefix, get_option('sysconfdir'))

control_center_pkgdatadir = join_paths(control_center_datadir, meson.project_name())
control_center_desktopdir = join_paths(control_center_datadir, 'applications')
control_center_icondir = join_paths(control_center_datadir, 'icons')
control_center_schemadir = join_paths (control_center_datadir, 'glib-2.0', 'schemas')

control_center_gettext = meson.project_name() + '-2.0'

host_is_linux = host_machine.system().contains('linux')
host_is_linux_not_s390 = host_is_linux and not host_machine.cpu().contains('s390')

cc = meson.get_compiler('c')

config_h = configuration_data()

py = import('python')
python = py.find_installation('python3')
blueprint_compiler = find_program('blueprint-compiler', version: '>= 0.17')

config_h.set_quoted('TEST_NM_PYTHON', python.full_path())

application_id = 'org.gnome.Settings'

if get_option('profile') == 'development'
    application_id += '.' + 'Devel'
endif

version_split = meson.project_version().split('.')

# defines
set_defines = [
  # package
  ['APPLICATION_ID', application_id],
  ['PACKAGE', meson.project_name()],
  ['PACKAGE_VERSION', meson.project_version()],
  ['VERSION', meson.project_version()],
  ['MAJOR_VERSION', version_split[0]],
  ['PROFILE', get_option('profile')],
  # i18n
  ['GETTEXT_PACKAGE', control_center_gettext]
]

foreach define: set_defines
  config_h.set_quoted(define[0], define[1])
endforeach

distributor_logo = get_option('distributor_logo')
if (distributor_logo != '')
  config_h.set_quoted('DISTRIBUTOR_LOGO', distributor_logo,
                      description: 'Define to absolute path of distributor logo')
  dark_mode_distributor_logo = get_option('dark_mode_distributor_logo')
  if (dark_mode_distributor_logo != '')
    config_h.set_quoted('DARK_MODE_DISTRIBUTOR_LOGO', dark_mode_distributor_logo,
                        description: 'Define to absolute path of distributor logo for use in dark mode')
  endif
endif

# meson does not support octal values, so it must be handled as a
# string. See: https://github.com/mesonbuild/meson/issues/2047
config_h.set('USER_DIR_MODE', '0700',
             description: 'Permissions for creating the user\'s config, cache and data directories')

# compiler flags
common_flags = ['-DHAVE_CONFIG_H']

# We have so many deprecation warnings that it becomes impossible to notice any other useful
# warning, thus making the compiler output completely useless. Ideally, we should fix all
# of those but, until then, it's better to add this flag so that we avoid adding even more
# warnings by accident. If you want to fix some of those, just build Settings with:
# meson _build -Ddeprecated-declarions=enabled
if get_option('deprecated-declarations').disabled()
  common_flags += '-Wno-deprecated-declarations'
endif

# Only add this when optimizing is enabled (default)
optimized_src = '''
  #if __OPTIMIZE__ == 0
  #error No optimization
  #endif
'''

control_center_optimized = get_option('buildtype').contains('optimized') and cc.compiles(optimized_src)

if control_center_optimized
  common_flags += '-Wp,-D_FORTIFY_SOURCE=2'
endif

if get_option('buildtype').contains('debug')
  test_cflags = [
    '-Wcast-align',
    '-Wmissing-field-initializers',
    '-Wmissing-declarations',
    '-Wmissing-prototypes',
    '-Wnested-externs',
    '-Wno-strict-aliasing',
    '-Wno-sign-compare'
  ]

  common_flags += cc.get_supported_arguments(test_cflags)
endif

# Warnings which are not useful for our coding style, regardless of the build type
common_flags += cc.get_supported_arguments([
    '-Wno-unused-parameter',
])

add_project_arguments(common_flags, language: 'c')

# Check that we meet the dependencies
libgvc = subproject(
  'gvc',
  default_options: [
    'static=true',
    'alsa=false',
    'package_name=' + control_center_gettext,
    'package_version=' + meson.project_version(),
    'werror=false',
  ]
)
libgvc_dep = libgvc.get_variable('libgvc_dep')

libadwaita_version = '>= 1.8.alpha'
goa_req_version = '>= 3.51.0'
pulse_req_version = '>= 2.0'

gtk_dep = dependency(
  'gtk4',
  version: '>= 4.15.2',
  fallback: 'gtk',
  default_options: [
    'introspection=disabled',
    'build-demos=false',
    'build-testsuite=false',
    'build-tests=false',
    'build-examples=false',
  ]
)

libadwaita_dep = dependency(
  'libadwaita-1',
  version: libadwaita_version,
  required: false
)
# FIXME: this is a workaround because interactive-tests don't work with libadwaita as a subproject. See !1754
libadwaita_is_subproject = not libadwaita_dep.found()
if libadwaita_is_subproject
  libadwaita_dep = dependency(
    'libadwaita-1',
    version: libadwaita_version,
    fallback: 'libadwaita',
    default_options: [
      'examples=false',
      'introspection=disabled',
      'tests=false',
      'vapi=false',
      'werror=false',
    ],
  )
endif

tecla_dep = dependency(
  'tecla',
  version: '>=47.0',
  required: false
)

# Needs to be a subproject since tecla does not declare dependencies
tecla_is_subproject = not tecla_dep.found()
if tecla_is_subproject
  tecla = subproject(
    'tecla',
    default_options: [
      'werror=false',
    ],
  )
endif

subproject('libgxdp',
  default_options: [
    'tests=false',
    'werror=false',
  ],
)

accounts_dep = dependency('accountsservice', version: '>= 23.11.69')
colord_dep = dependency('colord', version: '>= 0.1.34')
gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0', version: '>= 2.23.0')
gio_dep = dependency('gio-2.0')
glib_dep = dependency('glib-2.0', version: '>= 2.76.6')
gnome_desktop_dep = dependency('gnome-desktop-4')
gnome_bg_dep = dependency('gnome-bg-4')
gnome_settings_dep = dependency('gnome-settings-daemon', version: '>= 48.rc', fallback: 'gsd')
goa_dep = dependency('goa-1.0', version: goa_req_version, fallback: 'goa')
gsettings_desktop_dep = dependency('gsettings-desktop-schemas', version: '>= 48.alpha')  # actually requires commit 9d55553, just after alpha
libxml_dep = dependency('libxml-2.0')
pulse_dep = dependency('libpulse', version: pulse_req_version)
pulse_mainloop_dep = dependency('libpulse-mainloop-glib', version: pulse_req_version)
upower_glib_dep = dependency('upower-glib', version: '>= 1.90.6')
gudev_dep = dependency('gudev-1.0', version: '>= 232')
epoxy_dep = dependency('epoxy')
gcr_dep = dependency('gcr-4', version: '>= 4.1.0')
pwquality_dep = dependency('pwquality', version: '>= 1.2.2')

m_dep = cc.find_library('m')

common_deps = [
  gio_dep,
  glib_dep,
  gsettings_desktop_dep,
  libadwaita_dep,
  dependency('gio-unix-2.0'),
  dependency('gthread-2.0'),
  gtk_dep,
]

polkit_gobject_dep = dependency('polkit-gobject-1', version: '>= 0.103')
# Also verify that polkit ITS files exist:
# https://gitlab.gnome.org/GNOME/gnome-control-center/-/issues/491
polkit_files = [ 'gettext/its/polkit.its', 'gettext/its/polkit.loc' ]
foreach polkit_file: polkit_files
  r = run_command('build-aux/meson/find_xdg_file.py', polkit_file, check: true)
  assert(r.returncode() == 0, 'ITS support missing from polkit, please upgrade or contact your distribution')
endforeach

# Localisation API
if cc.has_header_symbol('langinfo.h', '_NL_TIME_FIRST_WEEKDAY')
  config_h.set('HAVE__NL_TIME_FIRST_WEEKDAY', 1,
               description: 'Define if langinfo.h supports _NL_TIME_FIRST_WEEKDAY')
endif

# Check for CUPS 1.4 or newer
cups_dep = dependency('cups', version : '>= 1.4', required: false)
assert(cups_dep.found(), 'CUPS 1.4 or newer not found')

# https://bugzilla.gnome.org/show_bug.cgi?id=696766
cups_cflags = []
if cups_dep.version().version_compare('>= 1.6')
  cups_cflags += '-D_PPD_DEPRECATED='
endif

# cups headers
check_headers = [
  ['HAVE_CUPS_CUPS_H', 'cups/cups.h'],
  ['HAVE_CUPS_PPD_H', 'cups/ppd.h']
]

foreach header: check_headers
  assert(cc.has_header(header[1], args: cups_cflags), 'CUPS headers not found: ' + header[1])
endforeach

config_h.set10('HAVE_CUPS_HTTPCONNECT2',
               cc.has_function('httpConnect2', dependencies: cups_dep),
               description: 'Define if httpConnect2() is available in CUPS')

# X11
have_x11_support = get_option('x11')
if have_x11_support
  x11_dep = dependency('x11', version: '>= 1.8')
  xi_dep = dependency('xi', version: '>= 1.2')
endif
config_h.set('HAVE_X11', have_x11_support, description: 'Defined if X11 support is enabled')

# IBus support
enable_ibus = get_option('ibus')
if enable_ibus
  ibus_dep = dependency('ibus-1.0', version: '>= 1.5.2')
endif
config_h.set('HAVE_IBUS', enable_ibus,
             description: 'Defined if IBus support is enabled')

# thunderbolt
config_h.set10('HAVE_FN_EXPLICIT_BZERO',
               cc.has_function('explicit_bzero', prefix: '''#include <string.h>'''),
               description: 'Define if explicit_bzero is available')

# Snap support
enable_snap = get_option('snap')
if enable_snap
  libsoup_dep = dependency('libsoup-3.0')
endif
config_h.set('HAVE_SNAP', enable_snap,
             description: 'Define to 1 if Snap support is enabled')

# malcontent support
enable_malcontent = get_option('malcontent')
if enable_malcontent
  malcontent_dep = dependency('malcontent-0', version: '>= 0.7.0')
  config_h.set('HAVE_MALCONTENT_0_10', malcontent_dep.version().version_compare('>= 0.10.0'),
               description: 'Define to 1 if malcontent ≥ 0.10.0')
endif
config_h.set('HAVE_MALCONTENT', enable_malcontent,
             description: 'Define to 1 if malcontent support is enabled')

# location services support
location_services = get_option('location-services')
config_h.set('HAVE_LOCATION_SERVICES', location_services.enabled(),
             description: 'Whether location services is enabled')

# Snap support and wellbeing panel
json_glib_dep = dependency('json-glib-1.0')

if host_is_linux
  # ModemManager
  mm_dep =  dependency('mm-glib', version: '>= 0.7')
  # NetworkManager
  network_manager_deps = [
    dependency('libnm', version: '>= 1.52.0'),
    dependency('libnma-gtk4', version: '>= 1.10.2'),
    mm_dep,
  ]
endif
config_h.set('BUILD_NETWORK', host_is_linux,
             description: 'Define to 1 to build the Network panel')
config_h.set('HAVE_NETWORK_MANAGER', host_is_linux,
             description: 'Define to 1 if NetworkManager is available')
config_h.set('BUILD_WWAN', host_is_linux,
             description: 'Define to 1 to build the WWan panel')
config_h.set('HAVE_WWAN', host_is_linux,
             description: 'Define to 1 if WWan is available')

if host_is_linux_not_s390
  # gnome-bluetooth
  gnome_bluetooth_dep = dependency('gnome-bluetooth-ui-3.0')

  libwacom_dep = dependency('libwacom', version: '>= 1.4')

  wacom_deps = [
    libwacom_dep,
  ]
else
  message('Bluetooth and Wacom panels will not be built (no USB support on this platform)')
  message('Thunderbolt panel will not be built (not supported on this platform)')
endif
config_h.set('BUILD_BLUETOOTH', host_is_linux_not_s390,
             description: 'Define to 1 to build the Bluetooth panel')
config_h.set('HAVE_BLUETOOTH', host_is_linux_not_s390,
             description: 'Define to 1 if bluetooth support is available')
config_h.set('BUILD_WACOM', host_is_linux_not_s390,
             description: 'Define to 1 to build the Wacom panel')
config_h.set('HAVE_WACOM', host_is_linux_not_s390,
             description: 'Define to 1 if Wacom is supportted')
config_h.set('BUILD_THUNDERBOLT', host_is_linux_not_s390,
             description: 'Define to 1 to build the Thunderbolt panel')

fs = import('fs')
gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

po_dir = join_paths(meson.project_source_root(), 'po')
its_dir = join_paths(meson.project_source_root(), 'gettext')

install_subdir(
  'gettext',
  install_dir: control_center_datadir
)

top_inc = include_directories('.')
shell_inc = include_directories('shell')

subdir('data/icons')
subdir('po')
subdir('panels')
subdir('shell')
subdir('global-shortcuts-provider')
subdir('search-provider')

if get_option('tests')
  subdir('tests')
endif

if get_option('documentation')
  subdir('man')
endif

gnome.post_install(
  glib_compile_schemas: true,
  gtk_update_icon_cache: true,
)

configure_file(
  output: 'config.h',
  configuration: config_h
)

summary({
    'Documentation': get_option('documentation'),
    'Tests': get_option('tests'),
    'Optimized': control_center_optimized,
  },
  bool_yn: true,
)

summary({
    'GNOME Bluetooth': host_is_linux_not_s390,
    'NetworkManager': host_is_linux,
    'Wacom': host_is_linux_not_s390,
    'X11 xupport': have_x11_support,
  },
  section: 'Dependencies',
  bool_yn: true,
)

summary({
    'IBus': enable_ibus,
    'Snap': enable_snap,
    'Malcontent': enable_malcontent,
  },
  section: 'Optional Dependencies',
  bool_yn: true,
)
