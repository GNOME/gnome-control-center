env = [
  'GSETTINGS_BACKEND=memory',
  'LC_ALL=C',
  # Use the in-process AT context rather than the AT-SPI bus
  'GTK_A11Y=test',
]

exe = executable(
  'test-ua-typing-page',
  'test-ua-typing-page.c',
  dependencies: common_deps + [universal_access_panel_dep],
)

test(
  'test-ua-typing-page',
  exe,
  suite: 'universal-access',
  protocol: 'tap',
  env: env,
  timeout: 60,
)
