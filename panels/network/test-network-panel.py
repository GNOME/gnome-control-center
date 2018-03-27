#!/usr/bin/python3

import os
import sys
import unittest
import subprocess

# For simplicity, assume this display is not taken, g-s-d has more advanced
# code that should be cleaned up and shared
DISPLAY=':99'
xorg = 'Xvfb'

BUILDDIR = os.environ.get('BUILDDIR', os.path.join(os.path.dirname(__file__)))

try:
    import dbusmock
except ImportError:
    sys.stderr.write('You need python-dbusmock (http://pypi.python.org/pypi/python-dbusmock) for this test suite.\n')
    sys.exit(1)


class GSDTestCase(dbusmock.DBusTestCase):
    '''Base class for settings daemon tests

    This redirects the XDG directories to temporary directories, and runs local
    session and system D-BUSes with a minimal GNOME session and a mock
    notification daemon. It also provides common functionality for plugin
    tests.
    '''
    @classmethod
    def setUpClass(klass):
        os.environ['DISPLAY'] = DISPLAY
        os.environ['WAYLAND'] = ''

        klass.start_system_bus()
        klass.start_session_bus()

        klass.xorg = subprocess.Popen([xorg, DISPLAY, "-screen", "0", "1280x1024x24", "+extension", "GLX", '-terminate'])

    @classmethod
    def tearDownClass(klass):
        dbusmock.DBusTestCase.tearDownClass()

        klass.xorg.terminate()

    def test(self):
        test = subprocess.Popen([os.path.join(BUILDDIR, 'test-network-panel')])
        test.wait()
        return test.returncode == 0

# avoid writing to stderr
unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout, verbosity=2))
