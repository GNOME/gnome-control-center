#!/usr/bin/env python3

import os
import subprocess
import sys

icondir = os.path.join(sys.argv[1], 'icons', 'hicolor')

if not os.environ.get('DESTDIR'):
  print('Update icon cache...')
  subprocess.call(['gtk-update-icon-cache', '-f', '-t', icondir])
