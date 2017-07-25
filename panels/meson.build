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

if enable_bluetooth
  panels += 'bluetooth'
endif

if enable_network_manager
  panels += 'network'
endif

if enable_wacom
  panels += 'wacom'
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
