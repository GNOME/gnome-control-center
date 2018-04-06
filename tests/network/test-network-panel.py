#!/usr/bin/python3

import os
import sys
import unittest

try:
    import dbusmock
except ImportError:
    sys.stderr.write('You need python-dbusmock (http://pypi.python.org/pypi/python-dbusmock) for this test suite.\n')
    sys.exit(1)

# Add the shared directory to the search path
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'shared'))

from gtest import GTest
from x11session import X11SessionTestCase

BUILDDIR = os.environ.get('BUILDDIR', os.path.join(os.path.dirname(__file__)))


class PanelTestCase(X11SessionTestCase, GTest):
    g_test_exe = os.path.join(BUILDDIR, 'test-network-panel')


if __name__ == '__main__':
    # avoid writing to stderr
    unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout, verbosity=2))
