/* gnome-startup.c - Functions for handling one-time startups in sessions.
   Written by Tom Tromey <tromey@cygnus.com>.  */

#include <config.h>

#include "gnome-startup.h"

#include <string.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

gboolean
gnome_startup_acquire_token (const char *property,
			     const char *session_id)
{
  Atom atom, actual;
  unsigned long nitems, nbytes;
  unsigned char *current;
  int len, format;
  gboolean result;

  atom = XInternAtom (GDK_DISPLAY (), property, False);
  len = strlen (session_id);

  /* Append our session id to the property.  We do this to avoid a
     race condition: if two clients run this code, we want to make
     sure that only one client can acquire the lock.  */
  XChangeProperty (GDK_DISPLAY (), DefaultRootWindow (GDK_DISPLAY ()), atom,
		   XA_STRING, 8, PropModeAppend, session_id, len);

  if (XGetWindowProperty (GDK_DISPLAY (), DefaultRootWindow (GDK_DISPLAY ()),
			  atom, 0, len, False, XA_STRING,
			  &actual, &format,
			  &nitems, &nbytes, &current) != Success)
    current = NULL;

  if (! current)
    return 0;

  result = ! strncmp (current, session_id, len);
  XFree (current);

  return result;
}
