/*
 * Copyright (C) 2013 Intel, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *
 */
#include "cc-remote-login.h"
#include <gio/gio.h>

#define SSHD_SERVICE "sshd.service"

static void
active_state_ready_callback (GObject      *source_object,
                             GAsyncResult *result,
                             gpointer      gtkswitch)
{
  GVariant *active_variant, *tmp_variant;
  const gchar *active_state;
  gboolean active;
  GError *error = NULL;

  active_variant = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                                  result, &error);

  if (!active_variant)
    {
      g_warning ("Error getting remote login state: %s", error->message);
      g_clear_error (&error);
      return;
    }

  g_variant_get (active_variant, "(v)", &tmp_variant);
  active_state = g_variant_get_string (tmp_variant, NULL);

  active = g_str_equal (active_state, "active");

  g_variant_unref (active_variant);
  g_variant_unref (tmp_variant);

  /* set the switch to the correct state */
  g_object_set_data (G_OBJECT (gtkswitch), "set-from-dbus", GINT_TO_POINTER (1));
  gtk_switch_set_active (gtkswitch, active);
  gtk_widget_set_sensitive (gtkswitch, TRUE);
}

static void
path_ready_callback (GObject      *source_object,
                     GAsyncResult *result,
                     gpointer      gtkswitch)
{
  GVariant *path_variant;
  gchar *object_path;
  GError *error = NULL;

  path_variant = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                                result, &error);

  if (!path_variant)
    {
      g_warning ("Error getting remote login state: %s", error->message);
      g_clear_error (&error);
      return;
    }

  g_variant_get (path_variant, "(o)", &object_path);

  g_dbus_connection_call (G_DBUS_CONNECTION (source_object),
                          "org.freedesktop.systemd1",
                          object_path,
                          "org.freedesktop.DBus.Properties",
                          "Get",
                          g_variant_new ("(ss)",
                                         "org.freedesktop.systemd1.Unit",
                                         "ActiveState"),
                          (GVariantType*) "(v)",
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          active_state_ready_callback,
                          gtkswitch);


  g_variant_unref (path_variant);

}

static void
bus_ready_callback (GObject      *source_object,
                    GAsyncResult *result,
                    gpointer      gtkswitch)
{
  GDBusConnection *connection;
  GError *error = NULL;

  connection = g_bus_get_finish (result, &error);

  if (!connection)
    {
      g_warning ("Error getting remote login state: %s", error->message);
      g_clear_error (&error);
      return;
    }

  g_dbus_connection_call (connection,
                          "org.freedesktop.systemd1",
                          "/org/freedesktop/systemd1",
                          "org.freedesktop.systemd1.Manager",
                          "GetUnit",
                          g_variant_new ("(s)", SSHD_SERVICE),
                          (GVariantType*) "(o)",
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          NULL,
                          path_ready_callback,
                          gtkswitch);
}


void
cc_remote_login_get_enabled (GtkSwitch *gtkswitch)
{
  /* disable the switch until the current state is known */
  gtk_widget_set_sensitive (GTK_WIDGET (gtkswitch), FALSE);

  g_bus_get (G_BUS_TYPE_SYSTEM, NULL, bus_ready_callback, gtkswitch);
}


static gint std_err;

static void
child_watch_func (GPid     pid,
                  gint     status,
                  gpointer gtkswitch)
{
  if (status != 0)
    {
      g_warning ("Error enabling or disabling remote login service");

      /* make sure the switch reflects the current status */
      cc_remote_login_get_enabled (GTK_SWITCH (gtkswitch));
    }
  g_spawn_close_pid (pid);

  gtk_widget_set_sensitive (GTK_WIDGET (gtkswitch), TRUE);
}

void
cc_remote_login_set_enabled (GtkSwitch *gtkswitch)
{
  gchar *command[] = { "pkexec", LIBEXECDIR "/cc-remote-login-helper", NULL,
      NULL };
  GError *error = NULL;
  GPid pid;


  if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (gtkswitch), "set-from-dbus")) == 1)
    {
      g_object_set_data (G_OBJECT (gtkswitch), "set-from-dbus", NULL);
      return;
    }

  if (gtk_switch_get_active (gtkswitch))
    command[2] = "enable";
  else
    command[2] = "disable";

  gtk_widget_set_sensitive (GTK_WIDGET (gtkswitch), FALSE);

  g_spawn_async_with_pipes (NULL, command, NULL,
                            G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD, NULL,
                            NULL, &pid, NULL, NULL, &std_err, &error);

  g_child_watch_add (pid, child_watch_func, gtkswitch);

  if (error)
    {
      g_error ("Error running cc-remote-login-helper: %s", error->message);
      g_clear_error (&error);
    }
}
