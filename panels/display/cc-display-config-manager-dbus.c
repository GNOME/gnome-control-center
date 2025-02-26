/*
 * Copyright (C) 2017  Red Hat, Inc.
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

#include "cc-display-config-dbus.h"
#include "cc-display-config-manager-dbus.h"

#include <gio/gio.h>
#include <stdint.h>

struct _CcDisplayConfigManagerDBus
{
  CcDisplayConfigManager parent_instance;

  GDBusProxy *proxy;
  gulong properties_changed_id;

  GCancellable *cancellable;
  GDBusConnection *connection;
  guint monitors_changed_id;

  GVariant *current_state;

  gboolean apply_allowed;
  gboolean night_light_supported;

  GPtrArray *output_luminance;
};

typedef struct _LuminanceEntry
{
  char *connector;
  CcDisplayColorMode color_mode;
  double luminance;
  double default_luminance;
} LuminanceEntry;

G_DEFINE_TYPE (CcDisplayConfigManagerDBus,
               cc_display_config_manager_dbus,
               CC_TYPE_DISPLAY_CONFIG_MANAGER)

static void
luminance_entry_free (LuminanceEntry *entry)
{
  g_free (entry->connector);
  g_free (entry);
}

static LuminanceEntry *
luminance_entry_new (const char         *connector,
                     CcDisplayColorMode  color_mode,
                     double              luminance,
                     double              default_luminance)
{
  LuminanceEntry *entry;

  entry = g_new0 (LuminanceEntry, 1);
  entry->connector = g_strdup (connector);
  entry->color_mode = color_mode;
  entry->luminance = luminance;
  entry->default_luminance = default_luminance;

  return entry;
}

static gboolean
luminance_entry_matches (LuminanceEntry     *entry,
                         const char         *connector,
                         CcDisplayColorMode  color_mode)
{
  return (g_strcmp0 (entry->connector, connector) == 0 &&
          entry->color_mode == color_mode);
}

static LuminanceEntry *
find_luminance_entry (CcDisplayConfigManagerDBus *self,
                      const char                 *connector,
                      CcDisplayColorMode          color_mode)
{
  size_t i;

  for (i = 0; i < self->output_luminance->len; i++)
    {
      LuminanceEntry *entry =
        g_ptr_array_index (self->output_luminance, i);

      if (luminance_entry_matches (entry, connector, color_mode))
        return entry;
    }

  return NULL;
}

static CcDisplayConfig *
cc_display_config_manager_dbus_get_current (CcDisplayConfigManager *pself)
{
  CcDisplayConfigManagerDBus *self = CC_DISPLAY_CONFIG_MANAGER_DBUS (pself);

  if (!self->current_state)
    return NULL;

  return g_object_new (CC_TYPE_DISPLAY_CONFIG_DBUS,
                       "state", self->current_state,
                       "connection", self->connection, NULL);
}

static void
got_current_state (GObject      *object,
                   GAsyncResult *result,
                   gpointer      data)
{
  CcDisplayConfigManagerDBus *self;
  GVariant *variant;
  g_autoptr(GError) error = NULL;

  variant = g_dbus_connection_call_finish (G_DBUS_CONNECTION (object),
                                           result, &error);
  if (!variant)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          self = CC_DISPLAY_CONFIG_MANAGER_DBUS (data);
          g_clear_pointer (&self->current_state, g_variant_unref);
          _cc_display_config_manager_emit_changed (CC_DISPLAY_CONFIG_MANAGER (data));
          g_warning ("Error calling GetCurrentState: %s", error->message);
        }
      return;
    }

  self = CC_DISPLAY_CONFIG_MANAGER_DBUS (data);
  g_clear_pointer (&self->current_state, g_variant_unref);
  self->current_state = variant;

  _cc_display_config_manager_emit_changed (CC_DISPLAY_CONFIG_MANAGER (self));
}

static void
get_current_state (CcDisplayConfigManagerDBus *self)
{
  g_dbus_connection_call (self->connection,
                          "org.gnome.Mutter.DisplayConfig",
                          "/org/gnome/Mutter/DisplayConfig",
                          "org.gnome.Mutter.DisplayConfig",
                          "GetCurrentState",
                          NULL,
                          NULL,
                          G_DBUS_CALL_FLAGS_NO_AUTO_START,
                          -1,
                          self->cancellable,
                          got_current_state,
                          self);
}

static void
monitors_changed (GDBusConnection *connection,
                  const gchar     *sender_name,
                  const gchar     *object_path,
                  const gchar     *interface_name,
                  const gchar     *signal_name,
                  GVariant        *parameters,
                  gpointer         data)
{
  CcDisplayConfigManagerDBus *self = CC_DISPLAY_CONFIG_MANAGER_DBUS (data);
  get_current_state (self);
}

static void
ensure_output_luminance_entry (CcDisplayConfigManagerDBus *self,
                               const char                 *connector,
                               CcDisplayColorMode          color_mode,
                               double                      luminance,
                               double                      default_luminance)
{
  LuminanceEntry *entry;

  entry = find_luminance_entry (self, connector, color_mode);
  if (entry)
    {
      entry->luminance = luminance;
    }
  else
    {
      entry = luminance_entry_new (connector, color_mode,
                                   luminance, default_luminance);
      g_ptr_array_add (self->output_luminance, entry);
    }
}

static void
update_luminance (CcDisplayConfigManagerDBus *self)
{
  g_autoptr(GVariant) variant = NULL;
  GVariantIter iter;
  gpointer luminance_entry_pointer;

  variant = g_dbus_proxy_get_cached_property (self->proxy, "Luminance");
  g_return_if_fail (variant);

  g_variant_iter_init (&iter, variant);
  while (g_variant_iter_next (&iter, "@a{sv}", &luminance_entry_pointer))
    {
      g_autoptr(GVariant) luminance_entry = luminance_entry_pointer;
      char *connector = NULL;
      uint32_t color_mode_value;
      double luminance;
      double default_luminance;

      g_variant_lookup (luminance_entry, "connector", "&s", &connector);
      g_variant_lookup (luminance_entry, "color-mode", "u", &color_mode_value);

      g_variant_lookup (luminance_entry, "luminance", "d", &luminance);
      g_variant_lookup (luminance_entry, "default", "d", &default_luminance);

      ensure_output_luminance_entry (self, connector, color_mode_value,
                                     luminance, default_luminance);
    }
}

static double
cc_display_config_manager_dbus_get_luminance (CcDisplayConfigManager *pself,
                                              CcDisplayMonitor       *monitor,
                                              CcDisplayColorMode      color_mode)
{
  CcDisplayConfigManagerDBus *self = CC_DISPLAY_CONFIG_MANAGER_DBUS (pself);
  const char *connector = cc_display_monitor_get_connector_name (monitor);
  LuminanceEntry *entry = find_luminance_entry (self, connector, color_mode);

  g_return_val_if_fail (entry, 100.0);

  return entry->luminance;
}

static double
cc_display_config_manager_dbus_get_default_luminance (CcDisplayConfigManager *pself,
                                                      CcDisplayMonitor       *monitor,
                                                      CcDisplayColorMode      color_mode)
{
  CcDisplayConfigManagerDBus *self = CC_DISPLAY_CONFIG_MANAGER_DBUS (pself);
  const char *connector = cc_display_monitor_get_connector_name (monitor);
  LuminanceEntry *entry = find_luminance_entry (self, connector, color_mode);

  g_return_val_if_fail (entry, 100.0);

  return entry->default_luminance;
}

static void
cc_display_config_manager_dbus_set_luminance (CcDisplayConfigManager *pself,
                                              CcDisplayMonitor       *monitor,
                                              CcDisplayColorMode      color_mode,
                                              double                  new_luminance)
{
  CcDisplayConfigManagerDBus *self = CC_DISPLAY_CONFIG_MANAGER_DBUS (pself);
  const char *connector = cc_display_monitor_get_connector_name (monitor);
  LuminanceEntry *entry = find_luminance_entry (self, connector, color_mode);

  g_return_if_fail (entry);

  if (G_APPROX_VALUE (new_luminance, entry->default_luminance, DBL_EPSILON))
    {
      g_dbus_proxy_call (self->proxy,
                         "ResetLuminance",
                         g_variant_new ("(su)",
                                        connector,
                                        color_mode),
                         G_DBUS_CALL_FLAGS_NO_AUTO_START,
                         -1,
                         NULL, NULL, NULL);
    }
  else
    {
      g_dbus_proxy_call (self->proxy,
                         "SetLuminance",
                         g_variant_new ("(sud)",
                                        connector,
                                        color_mode,
                                        new_luminance),
                         G_DBUS_CALL_FLAGS_NO_AUTO_START,
                         -1,
                         NULL, NULL, NULL);
    }
}

static void
properties_changed (GDBusProxy                  *proxy,
                    GVariant                    *changed_properties,
                    char                       **invalidated_properties,
                    CcDisplayConfigManagerDBus  *self)
{
  update_luminance (self);
}

static void
bus_gotten (GObject      *object,
            GAsyncResult *result,
            gpointer      data)
{
  CcDisplayConfigManagerDBus *self;
  GDBusConnection *connection;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) variant = NULL;

  connection = g_bus_get_finish (result, &error);
  if (!connection)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          _cc_display_config_manager_emit_changed (CC_DISPLAY_CONFIG_MANAGER (data));
          g_warning ("Error obtaining DBus connection: %s", error->message);
        }
      return;
    }

  self = CC_DISPLAY_CONFIG_MANAGER_DBUS (data);
  self->connection = connection;
  self->monitors_changed_id =
    g_dbus_connection_signal_subscribe (self->connection,
                                        "org.gnome.Mutter.DisplayConfig",
                                        "org.gnome.Mutter.DisplayConfig",
                                        "MonitorsChanged",
                                        "/org/gnome/Mutter/DisplayConfig",
                                        NULL,
                                        G_DBUS_SIGNAL_FLAGS_NONE,
                                        monitors_changed,
                                        self,
                                        NULL);

  self->proxy = g_dbus_proxy_new_sync (self->connection,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       NULL,
                                       "org.gnome.Mutter.DisplayConfig",
                                       "/org/gnome/Mutter/DisplayConfig",
                                       "org.gnome.Mutter.DisplayConfig",
                                       NULL,
                                       &error);
  if (!self->proxy)
    {
      g_warning ("Failed to create D-Bus proxy to \"org.gnome.Mutter.DisplayConfig\": %s",
                 error->message);
      return;
    }

  self->properties_changed_id =
    g_signal_connect (self->proxy, "g-properties-changed",
                      G_CALLBACK (properties_changed),
                      self);
  update_luminance (self);

  variant = g_dbus_proxy_get_cached_property (self->proxy, "ApplyMonitorsConfigAllowed");
  if (variant)
    self->apply_allowed = g_variant_get_boolean (variant);
  else
    g_warning ("Missing property 'ApplyMonitorsConfigAllowed' on DisplayConfig API");

  variant = g_dbus_proxy_get_cached_property (self->proxy, "NightLightSupported");
  if (variant)
    self->night_light_supported = g_variant_get_boolean (variant);
  else
    g_warning ("Missing property 'NightLightSupported' on DisplayConfig API");

  get_current_state (self);
}

static void
cc_display_config_manager_dbus_init (CcDisplayConfigManagerDBus *self)
{
  self->apply_allowed = TRUE;
  self->night_light_supported = TRUE;
  self->cancellable = g_cancellable_new ();
  self->output_luminance =
    g_ptr_array_new_with_free_func ((GDestroyNotify) luminance_entry_free);

  g_bus_get (G_BUS_TYPE_SESSION, self->cancellable, bus_gotten, self);
}

static void
cc_display_config_manager_dbus_finalize (GObject *object)
{
  CcDisplayConfigManagerDBus *self = CC_DISPLAY_CONFIG_MANAGER_DBUS (object);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_ptr_array_unref (self->output_luminance);
  g_object_unref (self->proxy);

  if (self->monitors_changed_id && self->connection)
    g_dbus_connection_signal_unsubscribe (self->connection,
                                          self->monitors_changed_id);
  g_clear_object (&self->connection);
  g_clear_pointer (&self->current_state, g_variant_unref);

  G_OBJECT_CLASS (cc_display_config_manager_dbus_parent_class)->finalize (object);
}

static gboolean
cc_display_config_manager_dbus_get_apply_allowed (CcDisplayConfigManager *pself)
{
  CcDisplayConfigManagerDBus *self = CC_DISPLAY_CONFIG_MANAGER_DBUS (pself);

  return self->apply_allowed;
}

static gboolean
cc_display_config_manager_dbus_get_night_light_supported (CcDisplayConfigManager *pself)
{
  CcDisplayConfigManagerDBus *self = CC_DISPLAY_CONFIG_MANAGER_DBUS (pself);

  return self->night_light_supported;
}

static void
cc_display_config_manager_dbus_class_init (CcDisplayConfigManagerDBusClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  CcDisplayConfigManagerClass *parent_class = CC_DISPLAY_CONFIG_MANAGER_CLASS (klass);

  gobject_class->finalize = cc_display_config_manager_dbus_finalize;

  parent_class->get_current = cc_display_config_manager_dbus_get_current;
  parent_class->get_apply_allowed = cc_display_config_manager_dbus_get_apply_allowed;
  parent_class->get_night_light_supported = cc_display_config_manager_dbus_get_night_light_supported;
  parent_class->get_luminance = cc_display_config_manager_dbus_get_luminance;
  parent_class->get_default_luminance = cc_display_config_manager_dbus_get_default_luminance;
  parent_class->set_luminance = cc_display_config_manager_dbus_set_luminance;
}

CcDisplayConfigManager *
cc_display_config_manager_dbus_new (void)
{
  return g_object_new (CC_TYPE_DISPLAY_CONFIG_MANAGER_DBUS, NULL);
}
