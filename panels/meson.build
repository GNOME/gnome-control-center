subdir('common')

panels = [
  'applications',
  'background',
  'camera',
  'color',
  'datetime',
  'default-apps',
  'diagnostics',
  'display',
  'info-overview',
  'keyboard',
  'location',
  'lock',
  'microphone',
  'mouse',
  'notifications',
  'online-accounts',
  'power',
  'printers',
  'region',
  'removable-media',
  'search',
  'sharing',
  'sound',
  'universal-access',
  'usage',
  'user-accounts'
]

if host_is_linux
  panels += ['network']
endif

if host_is_linux_not_s390
  panels += [
    'bluetooth',
    'thunderbolt',
    'wacom'
  ]
endif

panels_list = []
panels_libs = []
foreach cappletname: panels
  cflags = [
    '-DG_LOG_DOMAIN="@0@-cc-panel"'.format(cappletname),
    '-DPANEL_ID="@0@"'.format(cappletname)
  ]

  subdir(cappletname)
endforeach
