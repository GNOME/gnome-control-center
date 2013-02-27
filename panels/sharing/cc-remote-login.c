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

typedef struct
{
  GtkSwitch    *gtkswitch;
  GtkWidget    *button;
  GCancellable *cancellable;
} CallbackData;

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
                             CallbackData *callback_data)
{
  GVariant *active_variant, *tmp_variant;
  const gchar *active_state;
  gboolean active;
  GError *error = NULL;

  active_variant = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                                  result, &error);

  if (!active_variant)
    {
      /* print a warning if there was an error but the operation was not
       * cancelled */
      if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Error getting remote login state: %s", error->message);

      g_clear_error (&error);
      g_free (callback_data);

      /* the switch will be remain insensitive, since the current state could
       * not be determined */
      return;
    }

  g_variant_get (active_variant, "(v)", &tmp_variant);
  active_state = g_variant_get_string (tmp_variant, NULL);

  active = g_str_equal (active_state, "active");

  g_variant_unref (active_variant);
  g_variant_unref (tmp_variant);

  /* set the switch to the correct state */
  if (callback_data->gtkswitch)
    set_switch_state (callback_data->gtkswitch, active);

  g_free (callback_data);
}

static void
path_ready_callback (GObject      *source_object,
                     GAsyncResult *result,
                     CallbackData *callback_data)
{
  GVariant *path_variant;
  gchar *object_path;
  GError *error = NULL;

  path_variant = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                                result, &error);

  if (!path_variant)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_free (callback_data);
          g_clear_error (&error);

          return;
        }

      /* this may fail if systemd or remote login service is not available */
      g_debug ("Error getting remote login state: %s", error->message);

      g_clear_error (&error);

      /* hide the remote login button, since the service is not available */
      if (callback_data->button)
        gtk_widget_hide (callback_data->button);

      g_free (callback_data);

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
                          callback_data->cancellable,
                          (GAsyncReadyCallback) active_state_ready_callback,
                          callback_data);

  g_variant_unref (path_variant);
}

static void
state_ready_callback (GObject      *source_object,
                      GAsyncResult *result,
                      CallbackData *callback_data)
{
  GVariant *state_variant;
  const gchar *state_string;
  GError *error = NULL;

  state_variant = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                                 result, &error);
  if (!state_variant)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_free (callback_data);
          g_clear_error (&error);

          return;
        }

      /* this may fail if systemd or remote login service is not available */
      g_debug ("Error getting remote login state: %s", error->message);

      g_clear_error (&error);

      /* hide the remote login button, since the service is not available */
      if (callback_data->button)
        gtk_widget_hide (callback_data->button);

      g_free (callback_data);

      return;
    }

  g_variant_get (state_variant, "(s)", &state_string);

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
                              (GAsyncReadyCallback) path_ready_callback,
                              callback_data);
    }
  else if (g_str_equal (state_string, "disabled"))
    {
      /* service is available, but is currently disabled */
      set_switch_state (callback_data->gtkswitch, FALSE);

      g_free (callback_data);
    }
  else
    {
      /* unknown state */
      g_warning ("Unknown state %s for %s", state_string, SSHD_SERVICE);

      g_free (callback_data);
    }

  g_variant_unref (state_variant);
}

static void
bus_ready_callback (GObject      *source_object,
                    GAsyncResult *result,
                    CallbackData *callback_data)
{
  GDBusConnection *connection;
  GError *error = NULL;

  connection = g_bus_get_finish (result, &error);

  if (!connection)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Error getting remote login state: %s", error->message);
      g_clear_error (&error);
      g_free (callback_data);

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
                          (GAsyncReadyCallback) state_ready_callback,
                          callback_data);
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
             (GAsyncReadyCallback) bus_ready_callback, callback_data);
}

static gint std_err;

static void
child_watch_func (GPid     pid,
                  gint     status,
                  gpointer user_data)
{
  CallbackData *callback_data = user_data;
  if (status != 0)
    {
      g_warning ("Error enabling or disabling remote login service");

      /* make sure the switch reflects the current status */
      cc_remote_login_get_enabled (callback_data->cancellable, callback_data->gtkswitch, NULL);
    }
  g_spawn_close_pid (pid);

  gtk_widget_set_sensitive (GTK_WIDGET (callback_data->gtkswitch), TRUE);

  g_free (user_data);
}

void
cc_remote_login_set_enabled (GCancellable *cancellable,
                             GtkSwitch    *gtkswitch)
{
  gchar *command[] = { "pkexec", LIBEXECDIR "/cc-remote-login-helper", NULL,
      NULL };
  GError *error = NULL;
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
    {
      g_error ("Error running cc-remote-login-helper: %s", error->message);
      g_clear_error (&error);
    }
}
