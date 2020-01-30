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
#include "cc-camera-panel.h"
#include "cc-camera-resources.h"
#include "cc-util.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

#define APP_PERMISSIONS_TABLE "devices"
#define APP_PERMISSIONS_ID "camera"

struct _CcCameraPanel
{
  CcPanel       parent_instance;

  GtkStack     *stack;
  GtkListBox   *camera_apps_list_box;

  GSettings    *privacy_settings;

  GDBusProxy   *perm_store;
  GVariant     *camera_apps_perms;
  GVariant     *camera_apps_data;
  GHashTable   *camera_app_switches;

  GtkSizeGroup *camera_icon_size_group;
};

CC_PANEL_REGISTER (CcCameraPanel, cc_camera_panel)

typedef struct
{
  CcCameraPanel *self;
  GtkWidget *widget;
  gchar *app_id;
  gboolean changing_state;
  gboolean pending_state;
} CameraAppStateData;

static void
camera_app_state_data_free (CameraAppStateData *data)
{
    g_free (data->app_id);
    g_slice_free (CameraAppStateData, data);
}

static void
on_perm_store_set_done (GObject      *source_object,
                        GAsyncResult *res,
                        gpointer      user_data)
{
  g_autoptr(GVariant) results = NULL;
  g_autoptr(GError) error = NULL;
  CameraAppStateData *data;

  results = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                      res,
                                      &error);
  if (results == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to store permissions: %s", error->message);
      return;
    }

  data = (CameraAppStateData *) user_data;
  data->changing_state = FALSE;
  gtk_switch_set_state (GTK_SWITCH (data->widget), data->pending_state);
}

static gboolean
on_camera_app_state_set (GtkSwitch *widget,
                         gboolean   state,
                         gpointer   user_data)
{
  CameraAppStateData *data = (CameraAppStateData *) user_data;
  GVariantBuilder builder;
  CcCameraPanel *self;
  GVariantIter iter;
  GVariant *params;
  const gchar *key;
  gchar **value;

  self = data->self;

  if (data->changing_state)
    return TRUE;

  data->changing_state = TRUE;
  data->pending_state = state;

  g_variant_iter_init (&iter, self->camera_apps_perms);
  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
  while (g_variant_iter_loop (&iter, "{&s^a&s}", &key, &value))
    {
      gchar *tmp = NULL;

      /* It's OK to drop the entry if it's not in expected format */
      if (g_strv_length (value) != 1)
        continue;

      if (g_strcmp0 (data->app_id, key) == 0)
        {
          tmp = value[0];
          value[0] = state ? "yes" : "no";
        }

      g_variant_builder_add (&builder, "{s^as}", key, value);

      if (tmp != NULL)
        value[0] = tmp;
    }

  params = g_variant_new ("(sbsa{sas}v)",
                          APP_PERMISSIONS_TABLE,
                          TRUE,
                          APP_PERMISSIONS_ID,
                          &builder,
                          self->camera_apps_data);

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
add_camera_app (CcCameraPanel *self,
                const gchar   *app_id,
                gboolean       enabled)
{
  g_autofree gchar *desktop_id = NULL;
  CameraAppStateData *data;
  GDesktopAppInfo *app_info;
  GtkWidget *box, *row, *w;
  GIcon *icon;

  w = g_hash_table_lookup (self->camera_app_switches, app_id);
  if (w != NULL)
    {
      gtk_switch_set_active (GTK_SWITCH (w), enabled);
      return;
    }

  desktop_id = g_strdup_printf ("%s.desktop", app_id);
  app_info = g_desktop_app_info_new (desktop_id);
  if (!app_info)
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
  gtk_container_add (GTK_CONTAINER (self->camera_apps_list_box), row);

  icon = g_app_info_get_icon (G_APP_INFO (app_info));
  w = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_widget_show (w);
  gtk_widget_set_halign (w, GTK_ALIGN_CENTER);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_size_group_add_widget (self->camera_icon_size_group, w);
  gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);

  w = gtk_label_new (g_app_info_get_name (G_APP_INFO (app_info)));
  gtk_widget_show (w);
  gtk_widget_set_margin_start (w, 12);
  gtk_widget_set_margin_end (w, 12);
  gtk_widget_set_halign (w, GTK_ALIGN_START);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_label_set_xalign (GTK_LABEL (w), 0);
  gtk_box_pack_start (GTK_BOX (box), w, TRUE, TRUE, 0);

  w = gtk_switch_new ();
  gtk_widget_show (w);
  gtk_switch_set_active (GTK_SWITCH (w), enabled);
  gtk_widget_set_halign (w, GTK_ALIGN_END);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);
  g_settings_bind (self->privacy_settings,
                   "disable-camera",
                   w,
                   "sensitive",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);
  g_hash_table_insert (self->camera_app_switches,
                       g_strdup (app_id),
                       g_object_ref (w));

  data = g_slice_new (CameraAppStateData);
  data->self = self;
  data->app_id = g_strdup (app_id);
  data->widget = w;
  data->changing_state = FALSE;
  g_signal_connect_data (G_OBJECT (w),
                         "state-set",
                         G_CALLBACK (on_camera_app_state_set),
                         data,
                         (GClosureNotify) camera_app_state_data_free,
                         0);
}

/* Steals permissions and permissions_data references */

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

static void
update_perm_store (CcCameraPanel *self,
                   GVariant      *permissions,
                   GVariant      *permissions_data)
{
  GVariantIter iter;
  const gchar *key;
  gchar **value;

  g_clear_pointer (&self->camera_apps_perms, g_variant_unref);
  self->camera_apps_perms = permissions;
  g_clear_pointer (&self->camera_apps_data, g_variant_unref);
  self->camera_apps_data = permissions_data;

  g_variant_iter_init (&iter, permissions);
  while (g_variant_iter_loop (&iter, "{&s^a&s}", &key, &value))
    {
      gboolean enabled;

      if (g_strv_length (value) != 1)
        {
          g_debug ("Permissions for %s in incorrect format, ignoring..", key);
          continue;
        }

      enabled = (g_strcmp0 (value[0], "no") != 0);

      add_camera_app (self, key, enabled);
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
on_perm_store_lookup_done (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  CcCameraPanel *self = user_data;
  GVariant *ret, *permissions, *permissions_data;
  g_autoptr(GError) error = NULL;

  ret = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                  res,
                                  &error);
  if (ret == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed fetch permissions from flatpak permission store: %s", error->message);
      return;
    }

  permissions = g_variant_get_child_value (ret, 0);
  permissions_data = g_variant_get_child_value (ret, 1);
  update_perm_store (user_data, permissions, permissions_data);

  g_signal_connect_object (source_object,
                           "g-signal",
                           G_CALLBACK (on_perm_store_signal),
                           self,
                           0);
}

static void
on_perm_store_ready (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
  CcCameraPanel *self;
  g_autoptr(GVariant) params = NULL;
  g_autoptr(GError) error = NULL;
  GDBusProxy *proxy;

  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to connect to flatpak permission store: %s", error->message);
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
cc_camera_panel_finalize (GObject *object)
{
  CcCameraPanel *self = CC_CAMERA_PANEL (object);

  g_clear_object (&self->privacy_settings);
  g_clear_object (&self->perm_store);
  g_clear_object (&self->camera_icon_size_group);
  g_clear_pointer (&self->camera_apps_perms, g_variant_unref);
  g_clear_pointer (&self->camera_apps_data, g_variant_unref);
  g_clear_pointer (&self->camera_app_switches, g_hash_table_unref);

  G_OBJECT_CLASS (cc_camera_panel_parent_class)->finalize (object);
}

static const char *
cc_camera_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/camera";
}

static void
cc_camera_panel_constructed (GObject *object)
{
  CcCameraPanel *self = CC_CAMERA_PANEL (object);
  GtkWidget *box, *widget;

  G_OBJECT_CLASS (cc_camera_panel_parent_class)->constructed (object);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_widget_show (box);

  widget = gtk_switch_new ();
  gtk_widget_show (widget);
  gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (box), widget, FALSE, FALSE, 4);

  g_settings_bind (self->privacy_settings, "disable-camera",
                   widget, "active",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);
  g_object_bind_property_full  (widget, "active",
                                self->stack, "visible-child-name",
                                G_BINDING_SYNC_CREATE,
                                to_child_name,
                                NULL,
                                NULL, NULL);

  cc_shell_embed_widget_in_header (cc_panel_get_shell (CC_PANEL (self)),
                                   box,
                                   GTK_POS_RIGHT);
}

static void
cc_camera_panel_class_init (CcCameraPanelClass *klass)
{
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  panel_class->get_help_uri = cc_camera_panel_get_help_uri;

  object_class->finalize = cc_camera_panel_finalize;
  object_class->constructed = cc_camera_panel_constructed;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/camera/cc-camera-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcCameraPanel, stack);
  gtk_widget_class_bind_template_child (widget_class, CcCameraPanel, camera_apps_list_box);
}

static void
cc_camera_panel_init (CcCameraPanel *self)
{
  g_resources_register (cc_camera_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (self->camera_apps_list_box,
                                cc_list_box_update_header_func,
                                NULL,
                                NULL);
  self->camera_icon_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

  self->privacy_settings = g_settings_new ("org.gnome.desktop.privacy");


  self->camera_app_switches = g_hash_table_new_full (g_str_hash,
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
