/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2018 Red Hat, Inc
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
 * Author: Matthias Clasen <mclasen@redhat.com>
 */

#include "list-box-helper.h"
#include "cc-location-panel.h"
#include "cc-location-resources.h"
#include "cc-util.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

#define LOCATION_ENABLED "enabled"
#define APP_PERMISSIONS_TABLE "location"
#define APP_PERMISSIONS_ID "location"

struct _CcLocationPanel
{
  CcPanel       parent_instance;

  GtkStack     *stack;
  GtkListBox   *location_apps_list_box;

  GSettings    *location_settings;

  GDBusProxy   *perm_store;
  GVariant     *location_apps_perms;
  GVariant     *location_apps_data;
  GHashTable   *location_app_switches;

  GtkSizeGroup *location_icon_size_group;
};

CC_PANEL_REGISTER (CcLocationPanel, cc_location_panel)

typedef struct
{
  CcLocationPanel *self;
  GtkWidget       *widget;
  gchar           *app_id;
  gboolean         changing_state;
  gboolean         pending_state;
} LocationAppStateData;

static void
location_app_state_data_free (LocationAppStateData *data)
{
    g_free (data->app_id);
    g_slice_free (LocationAppStateData, data);
}

static void
on_perm_store_set_done (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  g_autoptr(GVariant) results = NULL;
  g_autoptr(GError) error = NULL;
  LocationAppStateData *data;

  results = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                      res,
                                      &error);
  if (results == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to store permissions: %s", error->message);

      return;
    }

  data = (LocationAppStateData *) user_data;
  data->changing_state = FALSE;
  gtk_switch_set_state (GTK_SWITCH (data->widget), data->pending_state);
}

static gboolean
on_location_app_state_set (GtkSwitch *widget,
                           gboolean   state,
                           gpointer   user_data)
{
  LocationAppStateData *data = (LocationAppStateData *) user_data;
  CcLocationPanel *self = data->self;
  GVariant *params;
  GVariantIter iter;
  const gchar *key;
  gchar **value;
  GVariantBuilder builder;

  if (data->changing_state)
    return TRUE;

  data->changing_state = TRUE;
  data->pending_state = state;

  g_variant_iter_init (&iter, self->location_apps_perms);
  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
  while (g_variant_iter_loop (&iter, "{&s^a&s}", &key, &value))
    {
      /* It's OK to drop the entry if it's not in expected format */
      if (g_strv_length (value) < 2)
        continue;

      if (g_strcmp0 (data->app_id, key) == 0)
        value[0] = state ? "EXACT" : "NONE";

      g_variant_builder_add (&builder, "{s^as}", key, value);
    }

  params = g_variant_new ("(sbsa{sas}v)",
                          APP_PERMISSIONS_TABLE,
                          TRUE,
                          APP_PERMISSIONS_ID,
                          &builder,
                          self->location_apps_data);

  g_dbus_proxy_call (self->perm_store,
                     "Set",
                     params,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     cc_panel_get_cancellable (CC_PANEL (self)),
                     on_perm_store_set_done,
                     data);

  return TRUE;
}

static void
add_location_app (CcLocationPanel *self,
                  const gchar     *app_id,
                  gboolean         enabled,
                  gint64           last_used)
{
  LocationAppStateData *data;
  GDesktopAppInfo *app_info;
  GDateTime *t;
  GtkWidget *box, *row, *w;
  GIcon *icon;
  gchar *last_used_str;
  gchar *desktop_id;

  w = g_hash_table_lookup (self->location_app_switches, app_id);
  if (w != NULL)
    {
      gtk_switch_set_active (GTK_SWITCH (w), enabled);
      return;
    }

  desktop_id = g_strdup_printf ("%s.desktop", app_id);
  app_info = g_desktop_app_info_new (desktop_id);
  g_free (desktop_id);
  if (app_info == NULL)
      return;

  row = gtk_list_box_row_new ();
  gtk_widget_show (row);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_show (box);
  gtk_widget_set_margin_start (box, 12);
  gtk_widget_set_margin_end (box, 6);
  gtk_widget_set_margin_top (box, 12);
  gtk_widget_set_margin_bottom (box, 12);
  gtk_container_add (GTK_CONTAINER (row), box);
  gtk_widget_set_hexpand (box, TRUE);
  gtk_container_add (GTK_CONTAINER (self->location_apps_list_box), row);

  icon = g_app_info_get_icon (G_APP_INFO (app_info));
  w = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_widget_show (w);
  gtk_widget_set_halign (w, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_size_group_add_widget (self->location_icon_size_group, w);
  gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);

  w = gtk_label_new (g_app_info_get_name (G_APP_INFO (app_info)));
  gtk_widget_show (w);
  gtk_widget_set_margin_start (w, 12);
  gtk_widget_set_margin_end (w, 12);
  gtk_widget_set_halign (w, GTK_ALIGN_START);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_label_set_xalign (GTK_LABEL (w), 0);
  gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);

  t = g_date_time_new_from_unix_utc (last_used);
  last_used_str = cc_util_get_smart_date (t);
  w = gtk_label_new (last_used_str);
  gtk_widget_show (w);
  g_free (last_used_str);
  gtk_style_context_add_class (gtk_widget_get_style_context (w), "dim-label");
  gtk_widget_set_margin_start (w, 12);
  gtk_widget_set_margin_end (w, 12);
  gtk_widget_set_halign (w, GTK_ALIGN_END);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), w, TRUE, TRUE, 0);

  w = gtk_switch_new ();
  gtk_widget_show (w);
  gtk_switch_set_active (GTK_SWITCH (w), enabled);
  gtk_widget_set_halign (w, GTK_ALIGN_END);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);
  g_settings_bind (self->location_settings, LOCATION_ENABLED,
                   w, "sensitive",
                   G_SETTINGS_BIND_DEFAULT);
  g_hash_table_insert (self->location_app_switches,
                       g_strdup (app_id),
                       g_object_ref (w));

  data = g_slice_new (LocationAppStateData);
  data->self = self;
  data->app_id = g_strdup (app_id);
  data->widget = w;
  data->changing_state = FALSE;
  g_signal_connect_data (w,
                         "state-set",
                         G_CALLBACK (on_location_app_state_set),
                         data,
                         (GClosureNotify) location_app_state_data_free,
                         0);
}

static gboolean
to_child_name (GBinding     *binding,
               const GValue *from,
               GValue       *to,
               gpointer      user_data)
{
  if (g_value_get_boolean (from))
    g_value_set_string (to, "content");
  else
    g_value_set_string (to, "empty");
  return TRUE;
}

/* Steals permissions and permissions_data references */
static void
update_perm_store (CcLocationPanel *self,
                   GVariant        *permissions,
                   GVariant        *permissions_data)
{
  GVariantIter iter;
  const gchar *key;
  gchar **value;

  g_clear_pointer (&self->location_apps_perms, g_variant_unref);
  self->location_apps_perms = permissions;

  g_clear_pointer (&self->location_apps_data, g_variant_unref);
  self->location_apps_data = permissions_data;

  g_variant_iter_init (&iter, permissions);
  while (g_variant_iter_loop (&iter, "{&s^a&s}", &key, &value))
    {
      gboolean enabled;
      gint64 last_used;

      if (g_strv_length (value) < 2)
        {
          g_debug ("Permissions for %s in incorrect format, ignoring..", key);
          continue;
        }

      enabled = (g_strcmp0 (value[0], "NONE") != 0);
      last_used = g_ascii_strtoll (value[1], NULL, 10);

      add_location_app (self, key, enabled, last_used);
    }
}

static void
on_perm_store_signal (GDBusProxy *proxy,
                      gchar      *sender_name,
                      gchar      *signal_name,
                      GVariant   *parameters,
                      gpointer    user_data)
{
  GVariant *permissions, *permissions_data;

  if (g_strcmp0 (signal_name, "Changed") != 0)
    return;

  permissions = g_variant_get_child_value (parameters, 4);
  permissions_data = g_variant_get_child_value (parameters, 3);
  update_perm_store (user_data, permissions, permissions_data);
}

static void
on_perm_store_lookup_done(GObject *source_object,
                          GAsyncResult *res,
                          gpointer user_data)
{
  g_autoptr(GError) error = NULL;
  GVariant *ret, *permissions, *permissions_data;

  ret = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                  res,
                                  &error);
  if (ret == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed fetch permissions from flatpak permission store: %s",
                   error->message);
      return;
    }

  permissions = g_variant_get_child_value (ret, 0);
  permissions_data = g_variant_get_child_value (ret, 1);
  update_perm_store (user_data, permissions, permissions_data);

  g_signal_connect_object (source_object,
                           "g-signal",
                           G_CALLBACK (on_perm_store_signal),
                           user_data,
                           0);
}

static void
on_perm_store_ready (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  g_autoptr(GError) error = NULL;
  CcLocationPanel *self;
  GDBusProxy *proxy;
  GVariant *params;

  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("Failed to connect to flatpak permission store: %s",
                     error->message);

      return;
    }
  self = user_data;
  self->perm_store = proxy;

  params = g_variant_new ("(ss)",
                          APP_PERMISSIONS_TABLE,
                          APP_PERMISSIONS_ID);
  g_dbus_proxy_call (self->perm_store,
                     "Lookup",
                     params,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     cc_panel_get_cancellable (CC_PANEL (self)),
                     on_perm_store_lookup_done,
                     self);
}

static void
cc_location_panel_finalize (GObject *object)
{
  CcLocationPanel *self = CC_LOCATION_PANEL (object);

  g_clear_object (&self->location_settings);
  g_clear_object (&self->perm_store);
  g_clear_object (&self->location_icon_size_group);
  g_clear_pointer (&self->location_apps_perms, g_variant_unref);
  g_clear_pointer (&self->location_apps_data, g_variant_unref);
  g_clear_pointer (&self->location_app_switches, g_hash_table_unref);

  G_OBJECT_CLASS (cc_location_panel_parent_class)->finalize (object);
}

static const char *
cc_location_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/location";
}

static void
cc_location_panel_constructed (GObject *object)
{
  CcLocationPanel *self = CC_LOCATION_PANEL (object);
  GtkWidget *box, *widget;

  G_OBJECT_CLASS (cc_location_panel_parent_class)->constructed (object);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_show (box);

  widget = gtk_switch_new ();
  gtk_widget_show (widget);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (box), widget);

  g_settings_bind (self->location_settings,
                   LOCATION_ENABLED,
                   widget,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_object_bind_property_full  (widget,
                                "active",
                                self->stack,
                                "visible-child-name",
                                G_BINDING_SYNC_CREATE,
                                to_child_name,
                                NULL,
                                NULL, NULL);

  cc_shell_embed_widget_in_header (cc_panel_get_shell (CC_PANEL (self)),
                                   box,
                                   GTK_POS_RIGHT);
}

static void
cc_location_panel_class_init (CcLocationPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  panel_class->get_help_uri = cc_location_panel_get_help_uri;

  object_class->finalize = cc_location_panel_finalize;
  object_class->constructed = cc_location_panel_constructed;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/location/cc-location-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcLocationPanel, stack);
  gtk_widget_class_bind_template_child (widget_class, CcLocationPanel, location_apps_list_box);
}

static void
cc_location_panel_init (CcLocationPanel *self)
{
  g_resources_register (cc_location_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (self->location_apps_list_box,
                                cc_list_box_update_header_func,
                                NULL, NULL);
  self->location_icon_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);
  self->location_settings = g_settings_new ("org.gnome.system.location");

  self->location_app_switches = g_hash_table_new_full (g_str_hash,
                                                       g_str_equal,
                                                       g_free,
                                                       g_object_unref);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.impl.portal.PermissionStore",
                            "/org/freedesktop/impl/portal/PermissionStore",
                            "org.freedesktop.impl.portal.PermissionStore",
                            cc_panel_get_cancellable (CC_PANEL (self)),
                            on_perm_store_ready,
                            self);
}
