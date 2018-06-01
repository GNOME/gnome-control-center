/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2013 Intel, Inc
 * Copyright (C) 2011,2012 Red Hat, Inc
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */


#include "cc-hostname-entry.h"
#include "hostname-helper.h"

#include <polkit/polkit.h>

struct _CcHostnameEntry
{
  GtkEntry             parent;

  GDBusProxy          *hostnamed_proxy;
  guint                set_hostname_timeout_source_id;
};

G_DEFINE_TYPE (CcHostnameEntry, cc_hostname_entry, GTK_TYPE_ENTRY)

#define SET_HOSTNAME_TIMEOUT 1

static void
cc_hostname_entry_set_hostname (CcHostnameEntry *self)
{
  g_autofree gchar *hostname = NULL;
  g_autoptr(GVariant) pretty_result = NULL;
  g_autoptr(GVariant) static_result = NULL;
  g_autoptr(GError) pretty_error = NULL;
  g_autoptr(GError) static_error = NULL;
  const gchar *text;

  text = gtk_entry_get_text (GTK_ENTRY (self));

  g_debug ("Setting PrettyHostname to '%s'", text);
  pretty_result = g_dbus_proxy_call_sync (self->hostnamed_proxy,
                                          "SetPrettyHostname",
                                          g_variant_new ("(sb)", text, FALSE),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1, NULL, &pretty_error);
  if (pretty_result == NULL)
    g_warning ("Could not set PrettyHostname: %s", pretty_error->message);

  /* Set the static hostname */
  hostname = pretty_hostname_to_static (text, FALSE);
  g_assert (hostname);

  g_debug ("Setting StaticHostname to '%s'", hostname);
  static_result = g_dbus_proxy_call_sync (self->hostnamed_proxy,
                                          "SetStaticHostname",
                                          g_variant_new ("(sb)", hostname, FALSE),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1, NULL, &static_error);
  if (static_result == NULL)
    g_warning ("Could not set StaticHostname: %s", static_error->message);
}

static char *
get_hostname_property (CcHostnameEntry *self,
                       const char      *property)
{
  g_autoptr(GVariant) variant = NULL;

  if (!self->hostnamed_proxy)
    return g_strdup ("");

  variant = g_dbus_proxy_get_cached_property (self->hostnamed_proxy,
                                              property);
  if (!variant)
    {
      g_autoptr(GError) error = NULL;
      g_autoptr(GVariant) inner = NULL;

      /* Work around systemd-hostname not sending us back
       * the property value when changing values */
      variant = g_dbus_proxy_call_sync (self->hostnamed_proxy,
                                        "org.freedesktop.DBus.Properties.Get",
                                        g_variant_new ("(ss)", "org.freedesktop.hostname1", property),
                                        G_DBUS_CALL_FLAGS_NONE,
                                        -1,
                                        NULL,
                                        &error);
      if (variant == NULL)
        {
          g_warning ("Failed to get property '%s': %s", property, error->message);
          return NULL;
        }

      g_variant_get (variant, "(v)", &inner);
      return g_variant_dup_string (inner, NULL);
    }
  else
    {
      return g_variant_dup_string (variant, NULL);
    }
}

static char *
cc_hostname_entry_get_display_hostname (CcHostnameEntry  *self)
{
  g_autofree gchar *str = NULL;

  str = get_hostname_property (self, "PrettyHostname");

  /* Empty strings means that we need to fallback */
  if (str != NULL &&
      *str == '\0')
    return get_hostname_property (self, "Hostname");

  return g_steal_pointer (&str);
}

static gboolean
set_hostname_timeout (CcHostnameEntry *self)
{
  self->set_hostname_timeout_source_id = 0;

  cc_hostname_entry_set_hostname (self);

  return FALSE;
}

static void
remove_hostname_timeout (CcHostnameEntry *self)
{
  if (self->set_hostname_timeout_source_id)
    g_source_remove (self->set_hostname_timeout_source_id);

  self->set_hostname_timeout_source_id = 0;
}

static void
reset_hostname_timeout (CcHostnameEntry *self)
{
  remove_hostname_timeout (self);

  self->set_hostname_timeout_source_id = g_timeout_add_seconds (SET_HOSTNAME_TIMEOUT,
                                                                (GSourceFunc) set_hostname_timeout,
                                                                self);
}

static void
text_changed_cb (CcHostnameEntry *entry)
{
  reset_hostname_timeout (entry);
}

static void
cc_hostname_entry_dispose (GObject *object)
{
  CcHostnameEntry *self = CC_HOSTNAME_ENTRY (object);

  if (self->set_hostname_timeout_source_id)
    {
      remove_hostname_timeout (self);
      set_hostname_timeout (self);
    }

  g_clear_object (&self->hostnamed_proxy);

  G_OBJECT_CLASS (cc_hostname_entry_parent_class)->dispose (object);
}

static void
cc_hostname_entry_constructed (GObject *object)
{
  CcHostnameEntry *self = CC_HOSTNAME_ENTRY (object);
  GPermission *permission;
  g_autoptr(GError) error = NULL;
  g_autofree gchar *str = NULL;

  permission = polkit_permission_new_sync ("org.freedesktop.hostname1.set-static-hostname",
                                           NULL, NULL, NULL);

  /* Is hostnamed installed? */
  if (permission == NULL)
    {
      g_debug ("Will not show hostname, hostnamed not installed");

      gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);

      return;
    }

  if (g_permission_get_allowed (permission))
    gtk_widget_set_sensitive (GTK_WIDGET (self), TRUE);
  else
    {
      g_debug ("Not allowed to change the hostname");
      gtk_widget_set_sensitive (GTK_WIDGET (self), FALSE);
    }

  gtk_widget_set_sensitive (GTK_WIDGET (self),
                            g_permission_get_allowed (permission));

  self->hostnamed_proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SYSTEM,
                                                         G_DBUS_PROXY_FLAGS_NONE,
                                                         NULL,
                                                         "org.freedesktop.hostname1",
                                                         "/org/freedesktop/hostname1",
                                                         "org.freedesktop.hostname1",
                                                         NULL,
                                                         &error);

  /* This could only happen if the policy file was installed
   * but not hostnamed, which points to a system bug */
  if (self->hostnamed_proxy == NULL)
    {
      g_debug ("Couldn't get hostnamed to start, bailing: %s", error->message);
      return;
    }

  str = cc_hostname_entry_get_display_hostname (CC_HOSTNAME_ENTRY (self));

  if (str != NULL)
    gtk_entry_set_text (GTK_ENTRY (self), str);
  else
    gtk_entry_set_text (GTK_ENTRY (self), "");

  g_signal_connect (G_OBJECT (self), "changed", G_CALLBACK (text_changed_cb), self);
}

static void
cc_hostname_entry_class_init (CcHostnameEntryClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->constructed = cc_hostname_entry_constructed;
  object_class->dispose = cc_hostname_entry_dispose;
}

static void
cc_hostname_entry_init (CcHostnameEntry *self)
{
}

CcHostnameEntry *
cc_hostname_entry_new (void)
{
  return g_object_new (CC_TYPE_HOSTNAME_ENTRY, NULL);
}

gchar*
cc_hostname_entry_get_hostname (CcHostnameEntry *entry)
{
  return get_hostname_property (entry, "Hostname");
}
