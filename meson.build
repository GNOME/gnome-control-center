project(
  'gnome-control-center', 'c',
        version : '3.31.2',
        license : 'GPL2+',
  meson_version : '>= 0.43.0'
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

# Tracing
enable_tracing = get_option('tracing')

config_h = configuration_data()

py3 = import('python3')
python = py3.find_python()

config_h.set_quoted('TEST_NM_PYTHON', python.path())

# defines
set_defines = [
  # package
  ['PACKAGE', meson.project_name()],
  ['PACKAGE_VERSION', meson.project_version()],
  ['VERSION', meson.project_version()],
  # i18n
  ['GETTEXT_PACKAGE', control_center_gettext]
]

foreach define: set_defines
  config_h.set_quoted(define[0], define[1])
endforeach

# meson does not support octal values, so it must be handled as a
# string. See: https://github.com/mesonbuild/meson/issues/2047
config_h.set('USER_DIR_MODE', '0700',
             description: 'Permissions for creating the user\'s config, cache and data directories')

# compiler flags
common_flags = ['-DHAVE_CONFIG_H']

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

add_project_arguments(common_flags, language: 'c')

# Check that we meet the dependencies
libgvc = subproject(
  'gvc',
  default_options: [
    'static=true',
    'alsa=false',
  ]
)
libgvc_dep = libgvc.get_variable('libgvc_dep')

libhandy_dep = dependency('libhandy-0.0', version: '>= 0.0.4', required: false)
if not libhandy_dep.found()
  libhandy = subproject(
    'libhandy',
    default_options: [
      'examples=false',
      'glade_catalog=false',
      'introspection=false',
      'tests=false',
      'vapi=false',
    ]
  )
  libhandy_dep = libhandy.get_variable('libhandy_dep')
endif

goa_req_version = '>= 3.25.3'
pulse_req_version = '>= 2.0'

accounts_dep = dependency('accountsservice', version: '>= 0.6.39')
colord_dep = dependency('colord', version: '>= 0.1.34')
gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0', version: '>= 2.23.0')
gio_dep = dependency('gio-2.0')
glib_dep = dependency('glib-2.0', version: '>= 2.53.0')
gnome_desktop_dep = dependency('gnome-desktop-3.0', version: '>= 3.27.90')
gnome_settings_dep = dependency('gnome-settings-daemon', version: '>= 3.25.90')
goa_dep = dependency('goa-1.0', version: goa_req_version)
gsettings_desktop_dep = dependency('gsettings-desktop-schemas', version: '>= 3.31.0')
libxml_dep = dependency('libxml-2.0')
polkit_gobject_dep = dependency('polkit-gobject-1', version: '>= 0.103')
pulse_dep = dependency('libpulse', version: pulse_req_version)
pulse_mainloop_dep = dependency('libpulse-mainloop-glib', version: pulse_req_version)
upower_glib_dep = dependency('upower-glib', version: '>= 0.99.8')
x11_dep = dependency('x11')
xi_dep = dependency('xi', version: '>= 1.2')

m_dep = cc.find_library('m')

common_deps = [
  gio_dep,
  glib_dep,
  gsettings_desktop_dep,
  libhandy_dep,
  dependency('gio-unix-2.0'),
  dependency('gthread-2.0'),
  dependency('gtk+-3.0', version: '>= 3.22.20')
]

# Check for CUPS 1.4 or newer
cups_dep = dependency('cups', version : '>= 1.4', required: false)
assert(cups_dep.found(), 'CUPS 1.4 or newer not found')

# https://bugzilla.gnome.org/show_bug.cgi?id=696766
cups_cflags = []
if cups_dep.version().version_compare('>= 1.6')
  cups_cflags += '-D_PPD_DEPRECATED=""'
endif

# cups headers
check_headers = [
  ['HAVE_CUPS_CUPS_H', 'cups/cups.h'],
  ['HAVE_CUPS_PPD_H', 'cups/ppd.h']
]

foreach header: check_headers
  assert(cc.has_header(header[1], args: cups_cflags), 'CUPS headers not found: ' + header[1])
endforeach

# Optional dependency for the user accounts panel
enable_cheese = get_option('cheese')
if enable_cheese
  cheese_deps = [
    dependency('cheese', version: '>= 3.28.0'),
    dependency('cheese-gtk', version: '>= 3.5.91')
  ]
endif
config_h.set('HAVE_CHEESE', enable_cheese,
             description: 'Define to 1 to enable cheese webcam support')

# IBus support
enable_ibus = get_option('ibus')
if enable_ibus
  ibus_dep = dependency('ibus-1.0', version: '>= 1.5.2')
endif
config_h.set('HAVE_IBUS', enable_ibus,
             description: 'Defined if IBus support is enabled')

# wayland
enable_wayland = get_option('wayland')
if enable_wayland
  wayland_deps = [
    dependency('gdk-wayland-3.0'),
    dependency('gudev-1.0')
  ]
endif
config_h.set('HAVE_WAYLAND', enable_wayland,
             description: 'Define to 1 if Wayland is enabled')

# thunderbolt
config_h.set10('HAVE_FN_EXPLICIT_BZERO',
               cc.has_function('explicit_bzero', prefix: '''#include <string.h>'''),
               description: 'Define if explicit_bzero is available')

if host_is_linux
  # network manager
  network_manager_deps = [
    dependency('libnm', version: '>= 1.10.0'),
    dependency('libnma', version: '>= 1.8.0'),
    dependency('mm-glib', version: '>= 0.7')
  ]
endif
config_h.set('BUILD_NETWORK', host_is_linux,
             description: 'Define to 1 to build the Network panel')
config_h.set('HAVE_NETWORK_MANAGER', host_is_linux,
             description: 'Define to 1 if NetworkManager is available')

if host_is_linux_not_s390
  # gnome-bluetooth
  gnome_bluetooth_dep = dependency('gnome-bluetooth-1.0', version: '>= 3.18.2')

  libwacom_dep = dependency('libwacom', version: '>= 0.7')

  wacom_deps = [
    libwacom_dep,
  ]
  config_h.set('HAVE_WACOM_3D_STYLUS', libwacom_dep.version().version_compare('>= 0.27'),
               description: 'Define to 1 if libwacom provides definition for 3D styli')
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

# Check for info panel
gnome_session_libexecdir = get_option('gnome_session_libexecdir')
if gnome_session_libexecdir == ''
  gnome_session_libexecdir = control_center_libexecdir
endif

gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

desktop_conf = configuration_data()
desktop_conf.set('VERSION', meson.project_version())

po_dir = join_paths(meson.source_root(), 'po')
its_dir = join_paths(meson.source_root(), 'gettext')

install_subdir(
  'gettext',
  install_dir: control_center_datadir
)

top_inc = include_directories('.')
shell_inc = include_directories('shell')

subdir('build-aux')
subdir('data/icons')
subdir('po')
subdir('panels')
subdir('shell')
subdir('search-provider')
subdir('tests')

if get_option('documentation')
  subdir('man')
endif

configure_file(
  output: 'config.h',
  configuration: config_h
)

output = ''
output += '\n        ' + meson.project_name() + ' - ' + meson.project_version() + '\n'
output += '    ===================================\n'
output += ' Options \n'
output += '     Documentation .............................. ' + get_option('documentation').to_string() + '\n'
output += '     Tracing .................................... ' + enable_tracing.to_string() + '\n'
output += '     gnome-session libexecdir ................... ' + gnome_session_libexecdir + '\n'
output += '     Optimized .................................. ' + control_center_optimized.to_string() + '\n'
output += ' Panels \n'
output += '     GNOME Bluetooth (Bluetooth panel) .......... ' + host_is_linux_not_s390.to_string() + '\n'
output += '     Cheese (Users panel webcam support) ........ ' + enable_cheese.to_string() + '\n'
output += '     IBus (Region panel IBus support) ........... ' + enable_ibus.to_string() + '\n'
output += '     NetworkManager (Network panel) ............. ' + host_is_linux.to_string() + '\n'
output += '     Wacom (Wacom tablet panel) ................. ' + host_is_linux_not_s390.to_string() + '\n'
output += '     Wayland .................................... ' + enable_wayland.to_string() + '\n'

message(output)
