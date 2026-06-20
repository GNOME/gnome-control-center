#!/usr/bin/env python3
# Copyright © 2026 The GNOME Project contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import os
import sys
import unittest

# Add the shared directory to the search path
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'shared'))

from gtest import GTest
from x11session import X11SessionTestCase

BUILDDIR = os.environ.get('BUILDDIR', os.path.join(os.path.dirname(__file__)))


class WireguardTestCase(X11SessionTestCase, GTest):
    g_test_exe = os.path.join(BUILDDIR, 'test-wireguard')


if __name__ == '__main__':
    # avoid writing to stderr
    unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout, verbosity=2))
