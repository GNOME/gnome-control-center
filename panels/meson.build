subdir('common')

panels = [
  'applications',
  'background',
  'color',
  'display',
  'keyboard',
  'mouse',
  'multitasking',
  'notifications',
  'online-accounts',
  'power',
  'printers',
  'privacy',
  'search',
  'sharing',
  'sound',
  'system',
  'universal-access',
 'wwan',
]

if host_is_linux
  panels += ['network']
endif

if host_is_linux_not_s390
  panels += [
    'bluetooth',
    'wacom'
  ]
endif

panels_list = []
panels_libs = []
foreach cappletname: panels
  cflags = [
    '-DG_LOG_DOMAIN="cc-@0@-panel"'.format(cappletname),
    '-DPANEL_ID="@0@"'.format(cappletname)
  ]

  subdir(cappletname)
endforeach
