#!/usr/bin/python3

import os
import sys

# For simplicity, assume this display is not taken
DISPLAY=':99'
xorg = 'Xvfb'

import subprocess

Xorg = subprocess.Popen([xorg, DISPLAY, "-screen", "0", "1280x1024x24", "+extension", "GLX", '-terminate'])


os.environ['DISPLAY'] = DISPLAY
os.environ['WAYLAND'] = ''

BUILDDIR = os.environ.get('BUILDDIR', os.path.join(os.path.dirname(__file__)))

test = subprocess.Popen(['dbus-launch', '--exit-with-session', os.path.join(BUILDDIR, 'test-network-panel')])
test.wait()

Xorg.terminate()

sys.exit(test.returncode)
