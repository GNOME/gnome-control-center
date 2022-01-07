deps = wacom_deps + [
  gnome_desktop_dep,
  gnome_settings_dep,
  x11_dep,
  xi_dep
]

cflags += ['-DGNOMELOCALEDIR="@0@"'.format(control_center_localedir)]

test_cflags = cflags + ['-DFAKE_AREA']

wacom_gresource = gnome.compile_resources(
  'cc-' + cappletname + '-resources',
  cappletname + '.gresource.xml',
    source_dir : '.',
        c_name : 'cc_' + cappletname,
  dependencies : resource_data,
        export : true
)

subdir('calibrator')

panels_list += cappletname
desktop = 'gnome-@0@-panel.desktop'.format(cappletname)

desktop_in = configure_file(
          input : desktop + '.in.in',
         output : desktop + '.in',
  configuration : desktop_conf
)

i18n.merge_file(
         type : 'desktop',
        input : desktop_in,
       output : desktop,
       po_dir : po_dir,
      install : true,
  install_dir : control_center_desktopdir
)

common_sources = files(
  'cc-tablet-tool-map.c',
  'cc-wacom-button-row.c',
  'cc-wacom-device.c',
  'cc-wacom-mapping-panel.c',
  'cc-wacom-nav-button.c',
  'cc-wacom-page.c',
  'cc-wacom-stylus-page.c',
  'cc-wacom-tool.c',
  'gsd-wacom-key-shortcut-button.c'
)

resource_data = files(
  'calibrator/calibrator.ui',
  'calibrator/calibrator.css',
  'calibrator/target.svg',
  'button-mapping.ui',
  'gnome-wacom-properties.ui',
  'wacom-stylus-3btn.svg',
  'wacom-stylus-3btn-no-eraser.svg',
  'wacom-stylus-airbrush.svg',
  'wacom-stylus-art-pen.svg',
  'wacom-stylus-classic.svg',
  'wacom-stylus-inking.svg',
  'wacom-stylus-no-eraser.svg',
  'wacom-stylus-page.ui',
  'wacom-stylus.svg',
  'wacom-tablet-cintiq.svg',
  'wacom-tablet-pc.svg',
  'wacom-tablet.svg'
)

common_sources += wacom_gresource

sources = common_sources + files(
  'cc-' + cappletname + '-panel.c',
  'cc-drawing-area.c'
)

deps += libdevice_dep

incs = [
  top_inc,
  calibrator_inc
]

panels_libs += static_library(
  cappletname + '-properties',
              sources : sources,
  include_directories : incs,
         dependencies : deps,
               c_args : cflags,
            link_with : [ libwacom_calibrator ]
)

name = 'test-wacom'

sources = common_sources + files(name + '.c')

executable(
                  name,
               sources,
  include_directories : incs,
         dependencies : deps,
               c_args : test_cflags,
            link_with : [ libwacom_calibrator_test ]
)
