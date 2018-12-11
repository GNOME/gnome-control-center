/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Red Hat, Inc
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
#include "cc-privacy-panel.h"
#include "cc-privacy-resources.h"
#include "cc-util.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

#define REMEMBER_RECENT_FILES "remember-recent-files"
#define RECENT_FILES_MAX_AGE "recent-files-max-age"
#define REMOVE_OLD_TRASH_FILES "remove-old-trash-files"
#define REMOVE_OLD_TEMP_FILES "remove-old-temp-files"
#define OLD_FILES_AGE "old-files-age"
#define SEND_SOFTWARE_USAGE_STATS "send-software-usage-stats"
#define REPORT_TECHNICAL_PROBLEMS "report-technical-problems"
#define LOCATION_ENABLED "enabled"

#define APP_PERMISSIONS_TABLE "location"
#define APP_PERMISSIONS_ID "location"

struct _CcPrivacyPanel
{
  CcPanel     parent_instance;

  GtkDialog   *abrt_dialog;
  GtkLabel    *abrt_explanation_label;
  GtkLabel    *abrt_policy_link_label;
  GtkSwitch   *abrt_switch;
  GtkSwitch   *automatic_screen_lock_switch;
  GtkButton   *clear_recent_button;
  GtkButton   *empty_trash_button;
  GtkListBox  *list_box;
  GtkFrame    *location_apps_frame;
  GtkLabel    *location_apps_label;
  GtkListBox  *location_apps_list_box;
  GtkDialog   *location_dialog;
  GtkLabel    *location_label;
  GtkSwitch   *location_services_switch;
  GtkDialog   *camera_dialog;
  GtkLabel    *camera_label;
  GtkSwitch   *camera_switch;
  GtkDialog   *microphone_dialog;
  GtkLabel    *microphone_label;
  GtkSwitch   *microphone_switch;
  GtkComboBox *lock_after_combo;
  GtkLabel    *lock_after_label;
  GtkComboBox *purge_after_combo;
  GtkButton   *purge_temp_button;
  GtkSwitch   *purge_temp_switch;
  GtkSwitch   *purge_trash_switch;
  GtkDialog   *recent_dialog;
  GtkSwitch   *recently_used_switch;
  GtkComboBox *retain_history_combo;
  GtkLabel    *retain_history_label;
  GtkDialog   *screen_lock_dialog;
  GtkGrid     *screen_lock_dialog_grid;
  GtkSwitch   *show_notifications_switch;
  GtkDialog   *software_dialog;
  GtkSwitch   *software_usage_switch;
  GtkDialog   *trash_dialog;

  GSettings  *lockdown_settings;
  GSettings  *lock_settings;
  GSettings  *privacy_settings;
  GSettings  *notification_settings;
  GSettings  *location_settings;

  GtkListBoxRow *abrt_row;
  guint          abrt_watch_id;

  GCancellable *cancellable;

  GDBusProxy *gclue_manager;
  GDBusProxy *perm_store;
  GVariant *location_apps_perms;
  GVariant *location_apps_data;
  GHashTable *location_app_switches;

  GtkSizeGroup *location_icon_size_group;
};

CC_PANEL_REGISTER (CcPrivacyPanel, cc_privacy_panel)

static char *
get_os_name (void)
{
  char *buffer;
  char *name;

  name = NULL;

  if (g_file_get_contents ("/etc/os-release", &buffer, NULL, NULL))
    {
       char *start, *end;

       start = end = NULL;
       if ((start = strstr (buffer, "NAME=")) != NULL)
         {
           start += strlen ("NAME=");
           end = strchr (start, '\n');
         }

       if (start != NULL && end != NULL)
         {
           name = g_strndup (start, end - start);
         }

       g_free (buffer);
    }

  if (name && *name != '\0')
    {
      char *tmp;
      tmp = g_shell_unquote (name, NULL);
      g_free (name);
      name = tmp;
    }

  if (name == NULL)
    name = g_strdup ("GNOME");

  return name;
}

static char *
get_privacy_policy_url (void)
{
  char *buffer;
  char *url;

  url = NULL;

  if (g_file_get_contents ("/etc/os-release", &buffer, NULL, NULL))
    {
       char *start, *end;

       start = end = NULL;
       if ((start = strstr (buffer, "PRIVACY_POLICY_URL=")) != NULL)
         {
           start += strlen ("PRIVACY_POLICY_URL=");
           end = strchr (start, '\n');
         }

       if (start != NULL && end != NULL)
         {
           url = g_strndup (start, end - start);
         }

       g_free (buffer);
    }

  if (url && *url != '\0')
    {
      char *tmp;
      tmp = g_shell_unquote (url, NULL);
      g_free (url);
      url = tmp;
    }

  if (url == NULL)
    url = g_strdup ("http://www.gnome.org/privacy-policy");

  return url;
}

static void
update_lock_screen_sensitivity (CcPrivacyPanel *self)
{
  gboolean   locked;

  locked = g_settings_get_boolean (self->lockdown_settings, "disable-lock-screen");

  gtk_widget_set_sensitive (GTK_WIDGET (self->screen_lock_dialog_grid), !locked);
}

static void
on_lockdown_settings_changed (GSettings      *settings,
                              const char     *key,
                              CcPrivacyPanel *panel)
{
  if (g_str_equal (key, "disable-lock-screen") == FALSE)
    return;

  update_lock_screen_sensitivity (panel);
}

static gboolean
on_off_label_mapping_get (GValue   *value,
                          GVariant *variant,
                          gpointer  user_data)
{
  g_value_set_string (value, g_variant_get_boolean (variant) ? _("On") : _("Off"));

  return TRUE;
}

static GtkLabel *
get_on_off_label (GSettings *settings,
                  const gchar *key)
{
  GtkWidget *w;

  w = gtk_label_new ("");
  g_settings_bind_with_mapping (settings, key,
                                w, "label",
                                G_SETTINGS_BIND_GET,
                                on_off_label_mapping_get,
                                NULL,
                                NULL,
                                NULL);
  return GTK_LABEL (w);
}

static gboolean
abrt_label_mapping_get (GValue   *value,
                        GVariant *variant,
                        gpointer  user_data)
{
  g_value_set_string (value, g_variant_get_boolean (variant) ? _("Automatic") : _("Manual"));

  return TRUE;
}

static GtkWidget *
get_abrt_label (GSettings *settings,
                const gchar *key)
{
  GtkWidget *w;

  w = gtk_label_new ("");
  g_settings_bind_with_mapping (settings, key,
                                w, "label",
                                G_SETTINGS_BIND_GET,
                                abrt_label_mapping_get,
                                NULL,
                                NULL,
                                NULL);
  return w;
}

typedef struct
{
  GtkLabel *label;
  const gchar *key1;
  const gchar *key2;
} Label2Data;

static void
set_on_off_label2 (GSettings   *settings,
                   const gchar *key,
                   gpointer     user_data)
{
  Label2Data *data = user_data;
  gboolean v1, v2;

  v1 = g_settings_get_boolean (settings, data->key1);
  v2 = g_settings_get_boolean (settings, data->key2);

  gtk_label_set_label (data->label, (v1 || v2) ? _("On") : _("Off"));
}

static GtkLabel *
get_on_off_label2 (GSettings *settings,
                   const gchar *key1,
                   const gchar *key2)
{
  Label2Data *data;

  data = g_new (Label2Data, 1);
  data->label = GTK_LABEL (gtk_label_new (""));
  data->key1 = g_strdup (key1);
  data->key2 = g_strdup (key2);

  g_signal_connect (settings, "changed",
                    G_CALLBACK (set_on_off_label2), data);

  set_on_off_label2 (settings, key1, data);

  return data->label;
}

static GtkListBoxRow *
add_row (CcPrivacyPanel *self,
         const gchar    *label,
         GtkDialog      *dialog,
         GtkWidget      *status)
{
  GtkWidget *box, *row, *w;

  row = gtk_list_box_row_new ();
  gtk_widget_show (row);
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 20);
  gtk_widget_show (box);
  gtk_widget_set_margin_start (box, 20);
  gtk_widget_set_margin_end (box, 20);
  gtk_widget_set_margin_top (box, 18);
  gtk_widget_set_margin_bottom (box, 18);
  gtk_container_add (GTK_CONTAINER (row), box);
  g_object_set_data (G_OBJECT (row), "dialog", dialog);
  gtk_widget_set_hexpand (box, TRUE);
  gtk_container_add (GTK_CONTAINER (self->list_box), row);

  w = gtk_label_new (label);
  gtk_widget_show (w);
  gtk_widget_set_halign (w, GTK_ALIGN_START);
  gtk_widget_set_halign (w, GTK_ALIGN_START);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (w, TRUE);
  gtk_label_set_ellipsize (GTK_LABEL (w), PANGO_ELLIPSIZE_END);
  gtk_label_set_xalign (GTK_LABEL (w), 0.0f);
  gtk_box_pack_start (GTK_BOX (box), w, TRUE, TRUE, 0);
  gtk_widget_set_halign (status, GTK_ALIGN_END);
  gtk_widget_set_valign (status, GTK_ALIGN_CENTER);
  gtk_box_pack_end (GTK_BOX (box), status, FALSE, FALSE, 0);

  return GTK_LIST_BOX_ROW (row);
}

static void
lock_combo_changed_cb (GtkWidget      *widget,
                       CcPrivacyPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  guint delay;
  gboolean ret;

  /* no selection */
  ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX(widget), &iter);
  if (!ret)
    return;

  /* get entry */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
  gtk_tree_model_get (model, &iter,
                      1, &delay,
                      -1);
  g_settings_set (self->lock_settings, "lock-delay", "u", delay);
}

static void
set_lock_value_for_combo (GtkComboBox    *combo_box,
                          CcPrivacyPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  guint value;
  gint value_tmp, value_prev;
  gboolean ret;
  guint i;

  /* get entry */
  model = gtk_combo_box_get_model (combo_box);
  ret = gtk_tree_model_get_iter_first (model, &iter);
  if (!ret)
    return;

  value_prev = 0;
  i = 0;

  /* try to make the UI match the lock setting */
  g_settings_get (self->lock_settings, "lock-delay", "u", &value);
  do
    {
      gtk_tree_model_get (model, &iter,
                          1, &value_tmp,
                          -1);
      if (value == value_tmp ||
          (value_tmp > value_prev && value < value_tmp))
        {
          gtk_combo_box_set_active_iter (combo_box, &iter);
          return;
        }
      value_prev = value_tmp;
      i++;
    } while (gtk_tree_model_iter_next (model, &iter));

  /* If we didn't find the setting in the list */
  gtk_combo_box_set_active (combo_box, i - 1);
}

static void
add_screen_lock (CcPrivacyPanel *self)
{
  GtkLabel *label;

  label = get_on_off_label (self->lock_settings, "lock-enabled");
  gtk_widget_show (GTK_WIDGET (label));
  add_row (self, _("Screen Lock"), self->screen_lock_dialog, GTK_WIDGET (label));

  g_signal_connect (self->screen_lock_dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  g_settings_bind (self->lock_settings, "lock-enabled",
                   self->automatic_screen_lock_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->lock_settings, "lock-enabled",
                   self->lock_after_combo, "sensitive",
                   G_SETTINGS_BIND_GET);

  g_object_bind_property (self->lock_after_combo, "sensitive", self->lock_after_label, "sensitive", G_BINDING_DEFAULT);

  set_lock_value_for_combo (self->lock_after_combo, self);
  g_signal_connect (self->lock_after_combo, "changed",
                    G_CALLBACK (lock_combo_changed_cb), self);

  g_settings_bind (self->notification_settings, "show-in-lock-screen",
                   self->show_notifications_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
}

static void
update_location_label (CcPrivacyPanel *self)
{
  gboolean in_use = FALSE, on;
  const gchar *label;

  if (self->gclue_manager != NULL)
    {
      GVariant *variant;

      variant = g_dbus_proxy_get_cached_property (self->gclue_manager, "InUse");
      if (variant != NULL)
        {
          in_use = g_variant_get_boolean (variant);
          g_variant_unref (variant);
        }
    }

  if (in_use)
    {
      gtk_label_set_label (self->location_label, _("In use"));
      return;
    }

  on = g_settings_get_boolean (self->location_settings, LOCATION_ENABLED);
  label = on ? C_("Location services status", "On") :
               C_("Location services status", "Off");
  gtk_label_set_label (self->location_label, label);
}

static void
on_gclue_manager_props_changed (GDBusProxy *manager,
                                GVariant   *changed_properties,
                                GStrv       invalidated_properties,
                                gpointer    user_data)
{
  update_location_label (user_data);
}

static void
on_gclue_manager_ready (GObject *source_object,
                        GAsyncResult *res,
                        gpointer user_data)
{
  CcPrivacyPanel *self;
  GDBusProxy *proxy;
  GError *error = NULL;

  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to connect to Geoclue: %s", error->message);
      g_error_free (error);

      return;
    }
  self = user_data;
  self->gclue_manager = proxy;

  g_signal_connect_object (self->gclue_manager,
                           "g-properties-changed",
                           G_CALLBACK (on_gclue_manager_props_changed),
                           self,
                           0);

  update_location_label (self);
}

typedef struct
{
  CcPrivacyPanel *self;
  GtkWidget *widget;
  gchar *app_id;
  gboolean changing_state;
  gboolean pending_state;
} LocationAppStateData;

static void
location_app_state_data_free (LocationAppStateData *data)
{
    g_free (data->app_id);
    g_slice_free (LocationAppStateData, data);
}

static void
on_perm_store_set_done (GObject *source_object,
                        GAsyncResult *res,
                        gpointer user_data)
{
  LocationAppStateData *data;
  GVariant *results;
  GError *error = NULL;

  results = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                      res,
                                      &error);
  if (results == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to store permissions: %s", error->message);
      g_error_free (error);

      return;
    }
  g_variant_unref (results);

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
  CcPrivacyPanel *self = data->self;
  GVariant *params;
  GVariantIter iter;
  gchar *key;
  gchar **value;
  GVariantBuilder builder;

  if (data->changing_state)
    return TRUE;

  data->changing_state = TRUE;
  data->pending_state = state;

  g_variant_iter_init (&iter, self->location_apps_perms);
  g_variant_builder_init (&builder, G_VARIANT_TYPE_ARRAY);
  while (g_variant_iter_loop (&iter, "{s^as}", &key, &value))
    {
      gchar *tmp = NULL;

      if (g_strv_length (value) < 2)
        /* It's OK to drop the entry if it's not in expected format */
        continue;

      if (g_strcmp0 (data->app_id, key) == 0)
        {
          tmp = value[0];
          value[0] = state ? "EXACT" : "NONE";
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
                          self->location_apps_data);

  g_dbus_proxy_call (self->perm_store,
                     "Set",
                     params,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     self->cancellable,
                     on_perm_store_set_done,
                     data);

  return TRUE;
}

static void
add_location_app (CcPrivacyPanel *self,
                  const gchar    *app_id,
                  gboolean        enabled,
                  gint64          last_used)
{
  GDesktopAppInfo *app_info;
  char *desktop_id;
  GtkWidget *box, *row, *w;
  GIcon *icon;
  GDateTime *t;
  char *last_used_str;
  LocationAppStateData *data;

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
  g_signal_connect_data (G_OBJECT (w),
                         "state-set",
                         G_CALLBACK (on_location_app_state_set),
                         data,
                         (GClosureNotify) location_app_state_data_free,
                         0);
}

/* Steals permissions and permissions_data references */
static void
update_perm_store (CcPrivacyPanel *self,
                   GVariant *permissions,
                   GVariant *permissions_data)
{
  GVariantIter iter;
  gchar *key;
  gchar **value;
  GList *children;

  g_clear_pointer (&self->location_apps_perms, g_variant_unref);
  self->location_apps_perms = permissions;
  g_clear_pointer (&self->location_apps_data, g_variant_unref);
  self->location_apps_data = permissions_data;

  g_variant_iter_init (&iter, permissions);
  while (g_variant_iter_loop (&iter, "{s^as}", &key, &value))
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

  children = gtk_container_get_children (GTK_CONTAINER (self->location_apps_list_box));
  if (g_list_length (children) > 0)
    {
      gtk_widget_set_visible (GTK_WIDGET (self->location_apps_label), TRUE);
      gtk_widget_set_visible (GTK_WIDGET (self->location_apps_frame), TRUE);
    }
  g_list_free (children);
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
  GVariant *ret, *permissions, *permissions_data;
  GError *error = NULL;

  ret = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                  res,
                                  &error);
  if (ret == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed fetch permissions from flatpak permission store: %s",
                   error->message);
      g_error_free (error);

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
on_perm_store_ready (GObject *source_object,
                     GAsyncResult *res,
                     gpointer user_data)
{
  CcPrivacyPanel *self;
  GDBusProxy *proxy;
  GVariant *params;
  GError *error = NULL;

  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("Failed to connect to flatpak permission store: %s",
                     error->message);
      g_error_free (error);

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
                     self->cancellable,
                     on_perm_store_lookup_done,
                     self);
}

static void
add_location (CcPrivacyPanel *self)
{
  self->location_label = GTK_LABEL (gtk_label_new (""));
  gtk_widget_show (GTK_WIDGET (self->location_label));
  update_location_label (self);

  add_row (self,
           _("Location Services"),
           self->location_dialog,
           GTK_WIDGET (self->location_label));

  g_signal_connect (self->location_dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  g_settings_bind (self->location_settings, LOCATION_ENABLED,
                   self->location_services_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_signal_connect_object (self->location_settings, "changed::" LOCATION_ENABLED,
                           G_CALLBACK (update_location_label), self,
                           G_CONNECT_SWAPPED);

  self->location_app_switches = g_hash_table_new_full (g_str_hash,
                                                       g_str_equal,
                                                       g_free,
                                                       g_object_unref);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.GeoClue2",
                            "/org/freedesktop/GeoClue2/Manager",
                            "org.freedesktop.GeoClue2.Manager",
                            self->cancellable,
                            on_gclue_manager_ready,
                            self);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.impl.portal.PermissionStore",
                            "/org/freedesktop/impl/portal/PermissionStore",
                            "org.freedesktop.impl.portal.PermissionStore",
                            self->cancellable,
                            on_perm_store_ready,
                            self);
}

static void
update_camera_label (CcPrivacyPanel *self)
{
  if (g_settings_get_boolean (self->privacy_settings, "disable-camera"))
    gtk_label_set_label (self->camera_label, C_("Camera status", "Off"));
  else
    gtk_label_set_label (self->camera_label, C_("Camera status", "On"));
}

static void
add_camera (CcPrivacyPanel *self)
{
  self->camera_label = GTK_LABEL (gtk_label_new (""));
  gtk_widget_show (GTK_WIDGET (self->camera_label));
  update_camera_label (self);

  add_row (self,
           _("Camera"),
           self->camera_dialog,
           GTK_WIDGET (self->camera_label));

  g_signal_connect (self->camera_dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  g_settings_bind (self->privacy_settings, "disable-camera",
                   self->camera_switch, "active",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  g_signal_connect_object (self->privacy_settings, "changed::disable-camera",
                           G_CALLBACK (update_camera_label), self,
                           G_CONNECT_SWAPPED);
}

static void
update_microphone_label (CcPrivacyPanel *self)
{
  if (g_settings_get_boolean (self->privacy_settings, "disable-microphone"))
    gtk_label_set_label (self->microphone_label, C_("Microphone status", "Off"));
  else
    gtk_label_set_label (self->microphone_label, C_("Microphone status", "On"));
}

static void
add_microphone (CcPrivacyPanel *self)
{
  self->microphone_label = GTK_LABEL (gtk_label_new (""));
  gtk_widget_show (GTK_WIDGET (self->microphone_label));
  update_microphone_label (self);

  add_row (self,
           _("Microphone"),
           self->microphone_dialog,
           GTK_WIDGET (self->microphone_label));

  g_signal_connect (self->microphone_dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  g_settings_bind (self->privacy_settings, "disable-microphone",
                   self->microphone_switch, "active",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

  g_signal_connect_object (self->privacy_settings, "changed::disable-microphone",
                           G_CALLBACK (update_microphone_label), self,
                           G_CONNECT_SWAPPED);
}

static void
retain_history_combo_changed_cb (GtkWidget      *widget,
                                 CcPrivacyPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gint value;
  gboolean ret;

  /* no selection */
  ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX(widget), &iter);
  if (!ret)
    return;

  /* get entry */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
  gtk_tree_model_get (model, &iter,
                      1, &value,
                      -1);
  g_settings_set (self->privacy_settings, RECENT_FILES_MAX_AGE, "i", value);
}

static void
set_retain_history_value_for_combo (GtkComboBox    *combo_box,
                                    CcPrivacyPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gint value;
  gint value_tmp, value_prev;
  gboolean ret;
  guint i;

  /* get entry */
  model = gtk_combo_box_get_model (combo_box);
  ret = gtk_tree_model_get_iter_first (model, &iter);
  if (!ret)
    return;

  value_prev = 0;
  i = 0;

  /* try to make the UI match the setting */
  g_settings_get (self->privacy_settings, RECENT_FILES_MAX_AGE, "i", &value);
  do
    {
      gtk_tree_model_get (model, &iter,
                          1, &value_tmp,
                          -1);
      if (value == value_tmp ||
          (value > 0 && value_tmp > value_prev && value < value_tmp))
        {
          gtk_combo_box_set_active_iter (combo_box, &iter);
          return;
        }
      value_prev = value_tmp;
      i++;
    } while (gtk_tree_model_iter_next (model, &iter));

  /* If we didn't find the setting in the list */
  gtk_combo_box_set_active (combo_box, i - 1);
}

static void
clear_recent (CcPrivacyPanel *self)
{
  GtkRecentManager *m;

  m = gtk_recent_manager_get_default ();
  gtk_recent_manager_purge_items (m, NULL);
}

static void
add_usage_history (CcPrivacyPanel *self)
{
  GtkLabel *label;

  label = get_on_off_label (self->privacy_settings, REMEMBER_RECENT_FILES);
  gtk_widget_show (GTK_WIDGET (label));
  add_row (self, _("Usage & History"), self->recent_dialog, GTK_WIDGET (label));

  g_signal_connect (self->recent_dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  g_settings_bind (self->privacy_settings, REMEMBER_RECENT_FILES,
                   self->recently_used_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  set_retain_history_value_for_combo (self->retain_history_combo, self);
  g_signal_connect (self->retain_history_combo, "changed",
                    G_CALLBACK (retain_history_combo_changed_cb), self);

  g_settings_bind (self->privacy_settings, REMEMBER_RECENT_FILES,
                   self->retain_history_combo, "sensitive",
                   G_SETTINGS_BIND_GET);

  g_object_bind_property (self->retain_history_combo, "sensitive", self->retain_history_label, "sensitive", G_BINDING_DEFAULT);
  g_signal_connect_swapped (self->clear_recent_button, "clicked",
                            G_CALLBACK (clear_recent), self);

}

static void
purge_after_combo_changed_cb (GtkWidget      *widget,
                              CcPrivacyPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  guint value;
  gboolean ret;

  /* no selection */
  ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX(widget), &iter);
  if (!ret)
    return;

  /* get entry */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
  gtk_tree_model_get (model, &iter,
                      1, &value,
                      -1);
  g_settings_set (self->privacy_settings, OLD_FILES_AGE, "u", value);
}

static void
set_purge_after_value_for_combo (GtkComboBox    *combo_box,
                                 CcPrivacyPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  guint value;
  gint value_tmp, value_prev;
  gboolean ret;
  guint i;

  /* get entry */
  model = gtk_combo_box_get_model (combo_box);
  ret = gtk_tree_model_get_iter_first (model, &iter);
  if (!ret)
    return;

  value_prev = 0;
  i = 0;

  /* try to make the UI match the purge setting */
  g_settings_get (self->privacy_settings, OLD_FILES_AGE, "u", &value);
  do
    {
      gtk_tree_model_get (model, &iter,
                          1, &value_tmp,
                          -1);
      if (value == value_tmp ||
          (value_tmp > value_prev && value < value_tmp))
        {
          gtk_combo_box_set_active_iter (combo_box, &iter);
          return;
        }
      value_prev = value_tmp;
      i++;
    } while (gtk_tree_model_iter_next (model, &iter));

  /* If we didn't find the setting in the list */
  gtk_combo_box_set_active (combo_box, i - 1);
}

static gboolean
run_warning (GtkWindow *parent, char *prompt, char *text, char *button_title)
{
  GtkWidget *dialog;
  GtkWidget *button;
  int result;
  dialog = gtk_message_dialog_new (parent,
                                   0,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_NONE,
                                   NULL);
  g_object_set (dialog,
                "text", prompt,
                "secondary-text", text,
                NULL);
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
  gtk_dialog_add_button (GTK_DIALOG (dialog), button_title, GTK_RESPONSE_OK);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), FALSE);

  button = gtk_dialog_get_widget_for_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  gtk_style_context_add_class (gtk_widget_get_style_context (button), "destructive-action");

  result = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  return result == GTK_RESPONSE_OK;
}

static void
empty_trash (CcPrivacyPanel *self)
{
  GDBusConnection *bus;
  gboolean result;

  result = run_warning (GTK_WINDOW (self->trash_dialog), _("Empty all items from Trash?"),
                        _("All items in the Trash will be permanently deleted."),
                        _("_Empty Trash"));

  if (!result)
    return; 

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_dbus_connection_call (bus,
                          "org.gnome.SettingsDaemon.Housekeeping",
                          "/org/gnome/SettingsDaemon/Housekeeping",
                          "org.gnome.SettingsDaemon.Housekeeping",
                          "EmptyTrash",
                          NULL, NULL, 0, -1, NULL, NULL, NULL);
  g_object_unref (bus);
}

static void
purge_temp (CcPrivacyPanel *self)
{
  GDBusConnection *bus;
  gboolean result;

  result = run_warning (GTK_WINDOW (self->trash_dialog), _("Delete all the temporary files?"),
                        _("All the temporary files will be permanently deleted."),
                        _("_Purge Temporary Files"));

  if (!result)
    return; 

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_dbus_connection_call (bus,
                          "org.gnome.SettingsDaemon.Housekeeping",
                          "/org/gnome/SettingsDaemon/Housekeeping",
                          "org.gnome.SettingsDaemon.Housekeeping",
                          "RemoveTempFiles",
                          NULL, NULL, 0, -1, NULL, NULL, NULL);
  g_object_unref (bus);
}

static void
add_trash_temp (CcPrivacyPanel *self)
{
  GtkLabel *w;

  w = get_on_off_label2 (self->privacy_settings, REMOVE_OLD_TRASH_FILES, REMOVE_OLD_TEMP_FILES);
  gtk_widget_show (GTK_WIDGET (w));
  add_row (self, _("Purge Trash & Temporary Files"), self->trash_dialog, GTK_WIDGET (w));

  g_signal_connect (self->trash_dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  g_settings_bind (self->privacy_settings, REMOVE_OLD_TRASH_FILES,
                   self->purge_trash_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->privacy_settings, REMOVE_OLD_TEMP_FILES,
                   self->purge_temp_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  set_purge_after_value_for_combo (self->purge_after_combo, self);
  g_signal_connect (self->purge_after_combo, "changed",
                    G_CALLBACK (purge_after_combo_changed_cb), self);

  g_signal_connect_swapped (self->empty_trash_button, "clicked", G_CALLBACK (empty_trash), self);

  g_signal_connect_swapped (self->purge_temp_button, "clicked", G_CALLBACK (purge_temp), self);
}

static void
add_software (CcPrivacyPanel *self)
{
  GtkLabel *w;

  /* disable until all the points on the bug are fixed:
   * https://bugzilla.gnome.org/show_bug.cgi?id=726234 */
  return;

  w = get_on_off_label (self->privacy_settings, SEND_SOFTWARE_USAGE_STATS);
  gtk_widget_show (GTK_WIDGET (w));
  add_row (self, _("Software Usage"), self->software_dialog, GTK_WIDGET (w));

  g_signal_connect (self->software_dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  g_settings_bind (self->privacy_settings, SEND_SOFTWARE_USAGE_STATS,
                   self->software_usage_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
}

static void
abrt_appeared_cb (GDBusConnection *connection,
                  const gchar     *name,
                  const gchar     *name_owner,
                  gpointer         user_data)
{
  CcPrivacyPanel *self = user_data;
  g_debug ("ABRT appeared");
  gtk_widget_show (GTK_WIDGET (self->abrt_row));
}

static void
abrt_vanished_cb (GDBusConnection *connection,
                  const gchar     *name,
                  gpointer         user_data)
{
  CcPrivacyPanel *self = user_data;
  g_debug ("ABRT vanished");
  gtk_widget_hide (GTK_WIDGET (self->abrt_row));
}

static void
add_abrt (CcPrivacyPanel *self)
{
  GtkWidget *w;
  char *os_name, *url, *msg;

  w = get_abrt_label (self->privacy_settings, REPORT_TECHNICAL_PROBLEMS);
  gtk_widget_show (w);
  self->abrt_row = add_row (self, _("Problem Reporting"), self->abrt_dialog, GTK_WIDGET (w));
  gtk_widget_hide (GTK_WIDGET (self->abrt_row));

  g_signal_connect (self->abrt_dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  g_settings_bind (self->privacy_settings, REPORT_TECHNICAL_PROBLEMS,
                   self->abrt_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  os_name = get_os_name ();
  /* translators: '%s' is the distributor's name, such as 'Fedora' */
  msg = g_strdup_printf (_("Sending reports of technical problems helps us improve %s. Reports are sent anonymously and are scrubbed of personal data."),
                         os_name);
  g_free (os_name);
  gtk_label_set_text (self->abrt_explanation_label, msg);
  g_free (msg);

  url = get_privacy_policy_url ();
  if (!url)
    {
      g_debug ("Not watching for ABRT appearing, /etc/os-release lacks a privacy policy URL");
      return;
    }
  msg = g_strdup_printf ("<a href=\"%s\">%s</a>", url, _("Privacy Policy"));
  g_free (url);
  gtk_label_set_markup (self->abrt_policy_link_label, msg);
  g_free (msg);

  self->abrt_watch_id = g_bus_watch_name (G_BUS_TYPE_SYSTEM,
                                                "org.freedesktop.problems.daemon",
                                                G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                abrt_appeared_cb,
                                                abrt_vanished_cb,
                                                self,
                                                NULL);
}

static void
cc_privacy_panel_finalize (GObject *object)
{
  CcPrivacyPanel *self = CC_PRIVACY_PANEL (object);

  if (self->abrt_watch_id > 0)
    {
      g_bus_unwatch_name (self->abrt_watch_id);
      self->abrt_watch_id = 0;
    }

  g_cancellable_cancel (self->cancellable);
  g_clear_pointer ((GtkWidget **)&self->recent_dialog, gtk_widget_destroy);
  g_clear_pointer ((GtkWidget **)&self->screen_lock_dialog, gtk_widget_destroy);
  g_clear_pointer ((GtkWidget **)&self->location_dialog, gtk_widget_destroy);
  g_clear_pointer ((GtkWidget **)&self->trash_dialog, gtk_widget_destroy);
  g_clear_pointer ((GtkWidget **)&self->software_dialog, gtk_widget_destroy);
  g_clear_pointer ((GtkWidget **)&self->abrt_dialog, gtk_widget_destroy);
  g_clear_object (&self->lockdown_settings);
  g_clear_object (&self->lock_settings);
  g_clear_object (&self->privacy_settings);
  g_clear_object (&self->notification_settings);
  g_clear_object (&self->location_settings);
  g_clear_object (&self->gclue_manager);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->perm_store);
  g_clear_object (&self->location_icon_size_group);
  g_clear_pointer (&self->location_apps_perms, g_variant_unref);
  g_clear_pointer (&self->location_apps_data, g_variant_unref);
  g_clear_pointer (&self->location_app_switches, g_hash_table_unref);

  G_OBJECT_CLASS (cc_privacy_panel_parent_class)->finalize (object);
}

static const char *
cc_privacy_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/privacy";
}

static void
activate_row (CcPrivacyPanel *self,
              GtkListBoxRow  *row)
{
  GObject *w;
  GtkWidget *toplevel;

  w = g_object_get_data (G_OBJECT (row), "dialog");

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  gtk_window_set_transient_for (GTK_WINDOW (w), GTK_WINDOW (toplevel));
  gtk_window_set_modal (GTK_WINDOW (w), TRUE);
  gtk_window_present (GTK_WINDOW (w));
}

static void
cc_privacy_panel_init (CcPrivacyPanel *self)
{
  g_resources_register (cc_privacy_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();

  gtk_list_box_set_header_func (self->location_apps_list_box,
                                cc_list_box_update_header_func,
                                NULL, NULL);
  self->location_icon_size_group = gtk_size_group_new (GTK_SIZE_GROUP_BOTH);

  g_signal_connect_swapped (self->list_box, "row-activated",
                            G_CALLBACK (activate_row), self);

  gtk_list_box_set_header_func (self->list_box,
                                cc_list_box_update_header_func,
                                NULL, NULL);

  self->lockdown_settings = g_settings_new ("org.gnome.desktop.lockdown");
  self->lock_settings = g_settings_new ("org.gnome.desktop.screensaver");
  self->privacy_settings = g_settings_new ("org.gnome.desktop.privacy");
  self->notification_settings = g_settings_new ("org.gnome.desktop.notifications");
  self->location_settings = g_settings_new ("org.gnome.system.location");

  add_screen_lock (self);
  add_location (self);
  add_camera (self);
  add_microphone (self);
  add_usage_history (self);
  add_trash_temp (self);
  add_software (self);
  add_abrt (self);

  g_signal_connect (self->lockdown_settings, "changed",
                    G_CALLBACK (on_lockdown_settings_changed), self);
  update_lock_screen_sensitivity (self);
}

static void
cc_privacy_panel_class_init (CcPrivacyPanelClass *klass)
{
  GObjectClass   *oclass       = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  CcPanelClass   *panel_class  = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_privacy_panel_get_help_uri;

  oclass->finalize = cc_privacy_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/privacy/cc-privacy-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, abrt_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, abrt_explanation_label);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, abrt_policy_link_label);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, abrt_switch);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, automatic_screen_lock_switch);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, clear_recent_button);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, empty_trash_button);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, list_box);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, location_apps_frame);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, location_apps_label);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, location_apps_list_box);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, location_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, location_services_switch);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, camera_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, camera_switch);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, microphone_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, microphone_switch);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, lock_after_combo);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, lock_after_label);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, purge_after_combo);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, purge_temp_button);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, purge_temp_switch);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, purge_trash_switch);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, recent_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, recently_used_switch);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, retain_history_combo);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, retain_history_label);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, screen_lock_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, screen_lock_dialog_grid);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, show_notifications_switch);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, software_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, software_usage_switch);
  gtk_widget_class_bind_template_child (widget_class, CcPrivacyPanel, trash_dialog);
}
