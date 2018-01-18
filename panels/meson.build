subdir('common')

panels = [
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

if enable_linux
  panels += [
    'bluetooth',
    'network',
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
