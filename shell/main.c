/*
 * Copyright (c) 2009, 2010 Intel, Inc.
 * Copyright (c) 2010 Red Hat, Inc.
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Thomas Wood <thos@gnome.org>
 */

#include "config.h"

#include <stdlib.h>
#include <locale.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#ifdef GDK_WINDOWING_X11
#include <X11/Xlib.h>
#endif

#include "cc-log.h"
#include "cc-application.h"

static char **
get_current_desktops (void)
{
  const char *envvar;

  envvar = g_getenv ("XDG_CURRENT_DESKTOP");

  if (!envvar)
    return g_new0 (char *, 0 + 1);

  return g_strsplit (envvar, G_SEARCHPATH_SEPARATOR_S, 0);
}

static gboolean
is_supported_desktop (void)
{
  g_auto(GStrv) desktops = NULL;
  guint i;

  desktops = get_current_desktops ();
  for (i = 0; desktops[i] != NULL; i++)
    {
      /* This matches OnlyShowIn in gnome-control-center.desktop.in.in */
      if (g_ascii_strcasecmp (desktops[i], "GNOME") == 0)
        return TRUE;
    }

  return FALSE;
}

int
main (gint    argc,
      gchar **argv)
{
  g_autoptr(GtkApplication) application = NULL;

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  setlocale (LC_ALL, "");
  cc_log_init ();

  if (!is_supported_desktop ())
    {
      g_printerr ("Running gnome-control-center is only supported under GNOME and Unity, exiting\n");
      return 1;
    }

  application = cc_application_new ();

  return g_application_run (G_APPLICATION (application), argc, argv);
}
