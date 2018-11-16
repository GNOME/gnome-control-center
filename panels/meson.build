subdir('common')

panels = [
  'applications',
  'background',
  'color',
  'datetime',
  'display',
  'info',
  'keyboard',
  'mouse',
  'notifications',
  'online-accounts',
  'power',
  'printers',
  'privacy',
  'region',
  'search',
  'sharing',
  'sound',
  'universal-access',
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
