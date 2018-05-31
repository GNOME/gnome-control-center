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

#ifndef SSHD_SERVICE
#define SSHD_SERVICE "sshd.service"
#endif

typedef struct
{
  GtkSwitch    *gtkswitch;
  GtkWidget    *button;
  GCancellable *cancellable;
} CallbackData;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (CallbackData, g_free)

static void
set_switch_state (GtkSwitch *gtkswitch,
                  gboolean   active)
{
  if (gtk_switch_get_active (gtkswitch) != active)
    {
      g_object_set_data (G_OBJECT (gtkswitch), "set-from-dbus",
                         GINT_TO_POINTER (1));
      gtk_switch_set_active (gtkswitch, active);
    }
  gtk_widget_set_sensitive (GTK_WIDGET (gtkswitch), TRUE);
}

static void
active_state_ready_callback (GObject      *source_object,
                             GAsyncResult *result,
                             gpointer      user_data)
{
  g_autoptr(CallbackData) callback_data = user_data;
  g_autoptr(GVariant) active_variant = NULL;
  g_autoptr(GVariant) child_variant = NULL;
  g_autoptr(GVariant) tmp_variant = NULL;
  const gchar *active_state;
  gboolean active;
  g_autoptr(GError) error = NULL;

  active_variant = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                                  result, &error);

  if (!active_variant)
    {
      /* print a warning if there was an error but the operation was not
       * cancelled */
      if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Error getting remote login state: %s", error->message);

      /* the switch will be remain insensitive, since the current state could
       * not be determined */
      return;
    }

  child_variant = g_variant_get_child_value (active_variant, 0);
  tmp_variant = g_variant_get_variant (child_variant);
  active_state = g_variant_get_string (tmp_variant, NULL);

  active = g_str_equal (active_state, "active");

  /* set the switch to the correct state */
  if (callback_data->gtkswitch)
    set_switch_state (callback_data->gtkswitch, active);
}

static void
path_ready_callback (GObject      *source_object,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  g_autoptr(CallbackData) callback_data = user_data;
  g_autoptr(GVariant) path_variant = NULL;
  g_autoptr(GVariant) child_variant = NULL;
  const gchar *object_path;
  g_autoptr(GError) error = NULL;

  path_variant = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                                result, &error);

  if (!path_variant)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      /* this may fail if systemd or remote login service is not available */
      g_debug ("Error getting remote login state: %s", error->message);

      /* hide the remote login button, since the service is not available */
      if (callback_data->button)
        gtk_widget_hide (callback_data->button);

      return;
    }

  child_variant = g_variant_get_child_value (path_variant, 0);
  object_path = g_variant_get_string (child_variant, NULL);

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
                          callback_data->cancellable,
                          active_state_ready_callback,
                          callback_data);
  g_steal_pointer (&callback_data);
}

static void
state_ready_callback (GObject      *source_object,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  g_autoptr(CallbackData) callback_data = user_data;
  g_autoptr(GVariant) state_variant = NULL;
  g_autoptr(GVariant) child_variant = NULL;
  const gchar *state_string;
  g_autoptr(GError) error = NULL;

  state_variant = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                                 result, &error);
  if (!state_variant)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      /* this may fail if systemd or remote login service is not available */
      g_debug ("Error getting remote login state: %s", error->message);

      /* hide the remote login button, since the service is not available */
      if (callback_data->button)
        gtk_widget_hide (callback_data->button);

      return;
    }

  child_variant = g_variant_get_child_value (state_variant, 0);
  state_string = g_variant_get_string (child_variant, NULL);

  if (g_str_equal (state_string, "enabled"))
    {
      /* service is enabled, so check whether it is running or not */
      g_dbus_connection_call (G_DBUS_CONNECTION (source_object),
                              "org.freedesktop.systemd1",
                              "/org/freedesktop/systemd1",
                              "org.freedesktop.systemd1.Manager",
                              "GetUnit",
                              g_variant_new ("(s)", SSHD_SERVICE),
                              (GVariantType*) "(o)",
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,
                              callback_data->cancellable,
                              path_ready_callback,
                              callback_data);
      g_steal_pointer (&callback_data);
    }
  else if (g_str_equal (state_string, "disabled"))
    {
      /* service is available, but is currently disabled */
      set_switch_state (callback_data->gtkswitch, FALSE);
    }
  else
    {
      /* unknown state */
      g_warning ("Unknown state %s for %s", state_string, SSHD_SERVICE);
    }
}

static void
bus_ready_callback (GObject      *source_object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  g_autoptr(CallbackData) callback_data = user_data;
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GError) error = NULL;

  connection = g_bus_get_finish (result, &error);

  if (!connection)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Error getting remote login state: %s", error->message);

      return;
    }

  g_dbus_connection_call (connection,
                          "org.freedesktop.systemd1",
                          "/org/freedesktop/systemd1",
                          "org.freedesktop.systemd1.Manager",
                          "GetUnitFileState",
                          g_variant_new ("(s)", SSHD_SERVICE),
                          (GVariantType*) "(s)",
                          G_DBUS_CALL_FLAGS_NONE,
                          -1,
                          callback_data->cancellable,
                          state_ready_callback,
                          callback_data);
  g_steal_pointer (&callback_data);
}

void
cc_remote_login_get_enabled (GCancellable *cancellable,
                             GtkSwitch    *gtkswitch,
                             GtkWidget    *button)
{
  CallbackData *callback_data;

  /* disable the switch until the current state is known */
  gtk_widget_set_sensitive (GTK_WIDGET (gtkswitch), FALSE);

  callback_data = g_new (CallbackData, 1);
  callback_data->gtkswitch = gtkswitch;
  callback_data->button = button;
  callback_data->cancellable = cancellable;

  g_bus_get (G_BUS_TYPE_SYSTEM, callback_data->cancellable,
             bus_ready_callback, callback_data);
}

static gint std_err;

static void
child_watch_func (GPid     pid,
                  gint     status,
                  gpointer user_data)
{
  g_autoptr(CallbackData) callback_data = user_data;
  if (status != 0)
    {
      g_warning ("Error enabling or disabling remote login service");

      /* make sure the switch reflects the current status */
      cc_remote_login_get_enabled (callback_data->cancellable, callback_data->gtkswitch, NULL);
    }
  g_spawn_close_pid (pid);

  gtk_widget_set_sensitive (GTK_WIDGET (callback_data->gtkswitch), TRUE);
}

void
cc_remote_login_set_enabled (GCancellable *cancellable,
                             GtkSwitch    *gtkswitch)
{
  gchar *command[] = { "pkexec", LIBEXECDIR "/cc-remote-login-helper", NULL,
      NULL };
  g_autoptr(GError) error = NULL;
  GPid pid;
  CallbackData *callback_data;

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

  callback_data = g_new0 (CallbackData, 1);
  callback_data->gtkswitch = gtkswitch;
  callback_data->cancellable = cancellable;

  g_child_watch_add (pid, child_watch_func, callback_data);

  if (error)
    g_error ("Error running cc-remote-login-helper: %s", error->message);
}
