subdir('common')
#subdir('datetime')
if host_is_linux
  subdir('network')
endif

# FIXME: these tests don't work when libadwaita is a subproject. See !1754
if not libadwaita_dep.found()
  subdir('interactive-panels')
endif

subdir('printers')
subdir('info')
subdir('keyboard')
