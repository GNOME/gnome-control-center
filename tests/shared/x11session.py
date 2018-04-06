import os
import sys
import subprocess
from dbusmock import DBusTestCase

# Intended to be shared across projects, submitted for inclusion into
# dbusmock but might need to live elsewhere
# The pull request contains python 2 compatibility code.
#  https://github.com/martinpitt/python-dbusmock/pull/44

class X11SessionTestCase(DBusTestCase):
    #: The lock file for the X server
    _Xserver_lock = '/tmp/.X%d-lock'
    #: The first display number to try
    _Xserver_display = 99
    #: The X server to start
    Xserver = 'Xvfb'
    #: Further parameters for the X server
    Xserver_args = ['-screen', '0', '1280x1024x24', '+extension', 'GLX']
    #: Where to redirect the X stdout and stderr to. Set to None for debugging
    #: purposes if the X server is failing for some reason.
    Xserver_output = subprocess.DEVNULL

    @classmethod
    def setUpClass(klass):
        klass.start_xorg()
        klass.start_system_bus()
        klass.start_session_bus()

    @classmethod
    def start_xorg_on_display(klass, display):
        r, w = os.pipe()

        display_str = ":%d" % display
        klass.xorg = subprocess.Popen([klass.Xserver, display_str, '-displayfd', "%d" % w] + klass.Xserver_args,
                                      pass_fds=(w,),
                                      stdout=klass.Xserver_output,
                                      stderr=subprocess.STDOUT)
        os.close(w)

        # The X server will write "%d\n", we need to make sure to read the "\n".
        # If the server dies we get zero bytes reads as EOF is reached.
        Xserver_display = b''
        while True:
            tmp = os.read(r, 1024)
            Xserver_display += tmp

            # Break out if the read was empty or the line feed was read
            if not tmp or tmp[-1] == b'\n':
                break

        os.close(r)
        if not Xserver_display:
            # X server seems unable to open the display, make sure it really is dead
            klass.stop_xorg()
            return False

        # Remove whitespace (trailing \n)
        Xserver_display.strip()
        Xserver_display = int(Xserver_display)

        if Xserver_display != display:
            raise AssertionError('Server reported back display %s while it should be on display %s' % (Xserver_display, display))

        # Export information into our environment for tests to use
        os.environ['DISPLAY'] = display_str
        os.environ['WAYLAND'] = ''

        # Server should still be up and running at this point
        assert klass.xorg.poll() is None
        return True

    @classmethod
    def start_xorg(klass):
        # The method is straight forward. Check whether the lock file exists,
        # if not, try to open the X server. Try with the next display if this
        # fails because the server could not start up.
        display = klass._Xserver_display

        while True:
            if not os.path.exists(klass._Xserver_lock % display):
                if klass.start_xorg_on_display(display):
                    break

                # And try the next one
                display -= 1
                if display < 0:
                    raise AssertionError('Could not find free display')

    @classmethod
    def stop_xorg(klass):
        if hasattr(klass, 'xorg'):
            klass.xorg.terminate()
            klass.xorg.wait()
            del klass.xorg

    @classmethod
    def tearDownClass(klass):
        DBusTestCase.tearDownClass()

        klass.stop_xorg()
