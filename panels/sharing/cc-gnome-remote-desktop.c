/*
 * Copyright (C) 2018 Red Hat, Inc.
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
 */

#include "config.h"

#include "cc-gnome-remote-desktop.h"

const SecretSchema *
cc_grd_vnc_password_get_schema (void)
{
  static const SecretSchema grd_vnc_password_schema = {
    .name = "org.gnome.RemoteDesktop.VncPassword",
    .flags = SECRET_SCHEMA_NONE,
    .attributes = {
      { "password", SECRET_SCHEMA_ATTRIBUTE_STRING },
      { "NULL", 0 },
    },
  };

  return &grd_vnc_password_schema;
}

gboolean
cc_grd_get_is_auth_method_prompt (GValue   *value,
                                  GVariant *variant,
                                  gpointer  user_data)
{
  const char * auth_method;

  auth_method = g_variant_get_string (variant, NULL);

  if (g_strcmp0 (auth_method, "prompt") == 0)
    {
      g_value_set_boolean (value, TRUE);
    }
  else if (g_strcmp0 (auth_method, "password") == 0)
    {
      g_value_set_boolean (value, FALSE);
    }
  else
    {
      g_warning ("Unhandled VNC auth method %s", auth_method);
      g_value_set_boolean (value, FALSE);
    }

  return TRUE;
}

GVariant *
cc_grd_set_is_auth_method_prompt (const GValue       *value,
                                  const GVariantType *type,
                                  gpointer            user_data)
{
  char *auth_method;

  if (g_value_get_boolean (value))
    auth_method = "prompt";
  else
    auth_method = "password";

  return g_variant_new_string (auth_method);
}

gboolean
cc_grd_get_is_auth_method_password (GValue   *value,
                                    GVariant *variant,
                                    gpointer  user_data)
{
  const char *auth_method;

  auth_method = g_variant_get_string (variant, NULL);

  if (g_strcmp0 (auth_method, "prompt") == 0)
    {
      g_value_set_boolean (value, FALSE);
    }
  else if (g_strcmp0 (auth_method, "password") == 0)
    {
      g_value_set_boolean (value, TRUE);
    }
  else
    {
      g_warning ("Unhandled VNC auth method %s", auth_method);
      g_value_set_boolean (value, FALSE);
    }

  return TRUE;
}

GVariant *
cc_grd_set_is_auth_method_password (const GValue       *value,
                                    const GVariantType *type,
                                    gpointer            user_data)
{
  char *auth_method;

  if (g_value_get_boolean (value))
    auth_method = "password";
  else
    auth_method = "prompt";

  return g_variant_new_string (auth_method);
}

static void
on_password_stored (GObject      *source,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GtkEntry *entry = GTK_ENTRY (user_data);
  GError *error = NULL;

  if (!secret_password_store_finish (result, &error))
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Failed to store VNC password: %s", error->message);
          g_object_set_data (G_OBJECT (entry),
                             "vnc-password-cancellable", NULL);
        }
      g_error_free (error);
    }
  else
    {
      g_object_set_data (G_OBJECT (entry),
                         "vnc-password-cancellable", NULL);
    }
}

void
cc_grd_on_vnc_password_entry_notify_text (GtkEntry   *entry,
                                          GParamSpec *pspec,
                                          gpointer    user_data)
{
  GCancellable *cancellable;
  const char *password;

  cancellable = g_object_get_data (G_OBJECT (entry), "vnc-password-cancellable");
  if (cancellable)
    g_cancellable_cancel (cancellable);

  cancellable = g_cancellable_new ();
  g_object_set_data_full (G_OBJECT (entry),
                          "vnc-password-cancellable",
                          cancellable, g_object_unref);

  password = gtk_entry_get_text (entry);

  secret_password_store (CC_GRD_VNC_PASSWORD_SCHEMA,
                         SECRET_COLLECTION_DEFAULT,
                         "GNOME Remote Desktop VNC password",
                         password,
                         cancellable, on_password_stored, entry,
                         NULL);
}

void
cc_grd_update_password_entry (GtkEntry *entry)
{
  g_autoptr(GError) error = NULL;
  g_autofree gchar *password = NULL;

  password = secret_password_lookup_sync (CC_GRD_VNC_PASSWORD_SCHEMA,
                                          NULL, &error,
                                          NULL);
  if (error)
    g_warning ("Failed to get password: %s", error->message);

  if (password)
    gtk_entry_set_text (entry, password);
}
