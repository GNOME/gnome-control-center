/* gnome-startup.h - Functions for handling one-time startups in sessions.
   Written by Tom Tromey <tromey@cygnus.com>.  */

#ifndef GNOME_STARTUP_H
#define GNOME_STARTUP_H

#include <libgnome/gnome-defs.h>
#include <glib.h>

BEGIN_GNOME_DECLS

/* This function is used by configurator programs that set some global
   X server state.  The general idea is that such a program can be run
   in an `initialization' mode, where it sets the server state
   according to some saved state.  This function implements a mutex
   for such programs.  The mutex a property on the root window.  Call
   this function with the session manager's session id.  If it returns
   false, then do nothing.  If it returns true, then the caller has
   obtained the mutex and should proceed with initialization.  The
   property name is generally of the form GNOME_<PROGRAM>_PROPERTY.  */

gboolean gnome_startup_acquire_token (const gchar *property_name,
				      const gchar *sm_id);

END_GNOME_DECLS

#endif /* GNOME_STARTUP_H */
