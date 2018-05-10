#!/usr/bin/env python3

import os
import subprocess
import sys

gsettingsschemadir = os.path.join(sys.argv[1], 'glib-2.0', 'schemas')
icondir = os.path.join(sys.argv[1], 'icons', 'hicolor')

if not os.environ.get('DESTDIR'):
    print('Compiling gsettings schemas...')
    subprocess.call(['glib-compile-schemas', gsettingsschemadir])

    print('Update icon cache...')
    subprocess.call(['gtk-update-icon-cache', '-f', '-t', icondir])
