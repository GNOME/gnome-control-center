#!/usr/bin/python3

import os
import sys
import unittest
import subprocess
import functools

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

########################
# These are generic helpers to make running glib tests nicer.
# Splits each test into a separate function and only shows the test output if
# an error occured.
class _GTestSingleProp:
    """Property which creates a bound method for calling the specified test."""
    def __init__(self, test):
        self.test = test

    @staticmethod
    def __func(self, test):
        self._gtest_single(test)

    def __get__(self, obj, cls):
        bound_method = self.__func.__get__(obj, cls)
        partial_method = functools.partial(bound_method, self.test)
        partial_method.__doc__ = bound_method.__doc__

        return partial_method

class GTestMeta(type):
    def __new__(cls, name, bases, namespace, **kwds):
        result = type.__new__(cls, name, bases, dict(namespace))

        if result.g_test_exe != None:
            try:
                GTestMeta.make_tests(result.g_test_exe, result)
            except Exception as e:
                print('')
                print(e)
                print('Error generating separate test funcs, will call binary once.')
                result.test_all = result._gtest_all

        return result

    def make_tests(exe, result):
        test = subprocess.Popen([exe, '-l'], stdout=subprocess.PIPE)
        stdout, stderr = test.communicate()

        stdout = stdout.decode('utf-8')

        for i, test in enumerate(stdout.split('\n')):
            if not test:
                continue

            # Number it and make sure the function name is prefixed with 'test'.
            # Keep the rest as is, we don't care about the fact that the function
            # names cannot be typed in.
            name = 'test_%03d_' % (i+1) + test
            setattr(result, name, _GTestSingleProp(test))


class GTest(metaclass = GTestMeta):
    """Helper class to run GLib test. A test function will be created for each
    test from the executable.

    Use by using this class as a mixin and setting g_test_exe to an appropriate
    value.
    """

    g_test_exe = None
    g_test_single_timeout = None
    g_test_all_timeout = None

    def _gtest_single(self, test):
        assert(test)
        p = subprocess.Popen([self.g_test_exe, '-q', '-p', test], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        try:
            stdout, stderr = p.communicate(timeout=self.g_test_single_timeout)
        except subprocess.TimeoutExpired:
            p.kill()
            stdout, stderr = p.communicate()
            stdout += b'\n\nTest was aborted due to timeout'

        try:
            stdout = stdout.decode('utf-8')
        except:
            pass

        if p.returncode != 0:
            self.fail(stdout)

    def _gtest_all(self):
        """Will be called if querying the individual tests fails"""
        subprocess.check_call([self.g_test_exe], timeout=self.g_test_all_timeout)

########################


class PanelTestCase(dbusmock.DBusTestCase, GTest):
    g_test_exe = os.path.join(BUILDDIR, 'test-network-panel')

    @classmethod
    def setUpClass(klass):
        os.environ['DISPLAY'] = DISPLAY
        os.environ['WAYLAND'] = ''

        klass.start_system_bus()
        klass.start_session_bus()

        klass.xorg = subprocess.Popen([xorg, DISPLAY, "-screen", "0", "1280x1024x24", "+extension", "GLX"])

    @classmethod
    def tearDownClass(klass):
        dbusmock.DBusTestCase.tearDownClass()

        klass.xorg.terminate()

if __name__ == '__main__':
    # avoid writing to stderr
    unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout, verbosity=2))
