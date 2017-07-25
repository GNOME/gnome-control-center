#!/usr/bin/env python3

import glob
import os
import re
import subprocess
import sys

icondir = os.path.join(sys.argv[1], 'icons', 'hicolor')

name_pattern = re.compile('hicolor_(?:apps)_(?:\d+x\d+|symbolic)_(.*)')
search_pattern = '/**/hicolor_*'

[os.rename(file, os.path.join(os.path.dirname(file), name_pattern.search(file).group(1)))
 for file in glob.glob(icondir + search_pattern, recursive=True)]

if not os.environ.get('DESTDIR'):
  print('Update icon cache...')
  subprocess.call(['gtk-update-icon-cache', '-f', '-t', icondir])
