/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2012 Giovanni Campagna <scampa.giovanni@gmail.com>
 * Copyright (C) 2015 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <string.h>
#include <glib/gi18n-lib.h>
#include <glib.h>
#include <gio/gio.h>
#include <gio/gdesktopappinfo.h>

#include "list-box-helper.h"
#include "cc-notifications-panel.h"
#include "cc-edit-dialog.h"

/*
 *  Key                       Switch
 *
 * "enable",                 "notifications-switch"                 When set to off, all other switches in the dialog are insensitive
 * "enable-sound-alerts",    "sound-alerts-switch"
 * "show-banners",           "notification-banners-switch"          Off and insensitive when corresponding panel switch is off
 * "force-expanded",         "notification-banners-content-switch"  Off and insensitive when switch above is off
 * "show-in-lock-screen",    "lock-screen-notifications-switch"     Off and insensitive when corresponding panel switch is off
 * "details-in-lock-screen", "lock-screen-content-switch"           Off and insensitive when switch above is off
 */

static void update_banner_switch (GtkWidget *dialog);
static void update_banner_content_switch (GtkWidget *dialog);
static void update_lock_screen_switch (GtkWidget *dialog);
static void update_lock_screen_content_switch (GtkWidget *dialog);
static void update_sound_switch (GtkWidget *dialog);
static void update_notification_switch (GtkWidget *dialog);
static void update_switches (GtkWidget *dialog);

static void
dialog_set_app_id (GtkWidget *dialog,
                   const char *app_id)
{
  g_object_set_data_full (G_OBJECT (dialog), "app-id", g_strdup (app_id), g_free);
}

static const char *
dialog_get_app_id (GtkWidget *dialog)
{
  return (const char*)g_object_get_data (G_OBJECT (dialog), "app-id");
}

static void
dialog_set_perm_store (GtkWidget  *dialog,
                       GDBusProxy *proxy)
{
  if (proxy)
    g_object_set_data_full (G_OBJECT (dialog), "perm-store", g_object_ref (proxy), g_object_unref);
}

static GDBusProxy *
dialog_get_perm_store (GtkWidget *dialog)
{
  return (GDBusProxy *)g_object_get_data (G_OBJECT (dialog), "perm-store");
}

static GtkWidget *
get_switch (GtkBuilder  *builder,
            const gchar *prefix)
{
  GtkWidget *result;
  gchar     *name;

  name = g_strdup_printf ("%s-switch", prefix);
  result = GTK_WIDGET (gtk_builder_get_object (builder, name));
  g_free (name);

  return result;
}

static void
set_key_from_switch (GtkWidget   *dialog,
                     const gchar *key,
                     GtkSwitch   *the_switch)
{
  GSettings *settings;

  settings = G_SETTINGS (g_object_get_data (G_OBJECT (dialog), "settings"));

  g_settings_set_boolean (settings, key, gtk_switch_get_active (the_switch));
}

static void
on_perm_store_set_done (GObject *source_object,
                        GAsyncResult *res,
                        gpointer user_data)
{
  g_autoptr(GVariant) results = NULL;
  g_autoptr(GError) error = NULL;

  results = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object),
                                      res,
                                      &error);
  if (results == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to store permissions: %s", error->message);
      return;
    }
}

static void
set_portal_permissions_for_app (GtkWidget *dialog, GtkSwitch *the_switch)
{
  GDBusProxy *perm_store = dialog_get_perm_store (dialog);
  const char *app_id = dialog_get_app_id (dialog);
  gboolean allow = gtk_switch_get_active (the_switch);
  g_autoptr(GVariant) perms = NULL;
  g_autoptr(GVariant) new_perms = NULL;
  g_autoptr(GVariant) data = NULL;
  GVariantBuilder builder;
  gboolean found;
  int i;
  const char *yes_strv[] = { "yes", NULL };
  const char *no_strv[] = { "no", NULL };
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) error = NULL;

  if (perm_store == NULL)
    {
      g_warning ("Could not find PermissionStore, not syncing notification permissions");
      return;
    }

  new_perms = g_variant_new_strv (allow ? yes_strv : no_strv, 1);
  g_variant_ref_sink (new_perms);

  g_variant_builder_init (&builder, G_VARIANT_TYPE("a{sas}"));
  found = FALSE;

  reply = g_dbus_proxy_call_sync (perm_store,
                                  "Lookup",
                                  g_variant_new ("(ss)",
                                                 "notifications",
                                                 "notification"),
                                  G_DBUS_CALL_FLAGS_NONE,
                                  -1,
                                  NULL,
                                  NULL);
  if (reply)
    {
      g_variant_get (reply, "(@a{sas}v)", &perms, &data);

      for (i = 0; i < g_variant_n_children (perms); i++)
        {
          const char *key;
          g_autoptr(GVariant) value = NULL;

          g_variant_get_child (perms, i, "{&s@as}", &key, &value);
          if (g_strcmp0 (key, app_id) == 0)
            {
              found = TRUE;
              g_variant_builder_add (&builder, "{s@as}", key, new_perms);
            }
          else
            g_variant_builder_add (&builder, "{s@as}", key, value);
        }
    }

  if (!found)
    g_variant_builder_add (&builder, "{s@as}", app_id, new_perms);


  g_dbus_proxy_call (perm_store,
                     "Set",
                     g_variant_new ("(sbsa{sas}v)",
                                    "notifications",
                                    TRUE,
                                    "notification",
                                    &builder,
                                    data ? data : g_variant_new_byte (0)),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     on_perm_store_set_done,
                     data);
}

static void
notifications_switch_state_set_cb (GtkSwitch  *widget,
                                   GParamSpec *pspec,
                                   GtkWidget  *dialog)
{
  set_key_from_switch (dialog, "enable", widget);
  set_portal_permissions_for_app (dialog, widget);
  update_sound_switch (dialog);
  update_banner_switch (dialog);
  update_banner_content_switch (dialog);
  update_lock_screen_switch (dialog);
  update_lock_screen_content_switch (dialog);
}

static void
sound_alerts_switch_state_set_cb (GtkSwitch  *widget,
                                  GParamSpec *pspec,
                                  GtkWidget  *dialog)
{
  set_key_from_switch (dialog, "enable-sound-alerts", widget);
}

static void
notification_banners_switch_state_set_cb (GtkSwitch  *widget,
                                          GParamSpec *pspec,
                                          GtkWidget  *dialog)
{
  set_key_from_switch (dialog, "show-banners", widget);
  update_banner_content_switch (dialog);
}

static void
notification_banners_content_switch_state_set_cb (GtkSwitch  *widget,
                                                  GParamSpec *pspec,
                                                  GtkWidget  *dialog)
{
  set_key_from_switch (dialog, "force-expanded", widget);
}

static void
lock_screen_notifications_switch_state_set_cb (GtkSwitch  *widget,
                                               GParamSpec *pspec,
                                               GtkWidget  *dialog)
{
  set_key_from_switch (dialog, "show-in-lock-screen", widget);
  update_lock_screen_content_switch (dialog);
}

static void
lock_screen_content_switch_state_set_cb (GtkSwitch  *widget,
                                         GParamSpec *pspec,
                                         GtkWidget  *dialog)
{
  set_key_from_switch (dialog, "details-in-lock-screen", widget);
}

static void
update_switches (GtkWidget *dialog)
{
  update_notification_switch (dialog);
  update_sound_switch (dialog);
  update_banner_switch (dialog);
  update_banner_content_switch (dialog);
  update_lock_screen_switch (dialog);
  update_lock_screen_content_switch (dialog);
}

static void
update_notification_switch (GtkWidget *dialog)
{
  GtkBuilder *builder;
  GSettings  *settings;
  GtkWidget  *widget;

  builder = GTK_BUILDER (g_object_get_data (G_OBJECT (dialog), "builder"));
  settings = G_SETTINGS (g_object_get_data (G_OBJECT (dialog), "settings"));

  widget = get_switch (builder, "notifications");
  g_signal_handlers_block_by_func (G_OBJECT (widget), notifications_switch_state_set_cb, dialog);
  gtk_switch_set_active (GTK_SWITCH (widget), g_settings_get_boolean (settings, "enable"));
  g_signal_handlers_unblock_by_func (G_OBJECT (widget), notifications_switch_state_set_cb, dialog);
}

static void
update_sound_switch (GtkWidget *dialog)
{
  GtkBuilder *builder;
  GSettings  *settings;
  GtkWidget  *widget;

  builder = GTK_BUILDER (g_object_get_data (G_OBJECT (dialog), "builder"));
  settings = G_SETTINGS (g_object_get_data (G_OBJECT (dialog), "settings"));

  widget = get_switch (builder, "sound-alerts");
  g_signal_handlers_block_by_func (G_OBJECT (widget), sound_alerts_switch_state_set_cb, dialog);
  gtk_switch_set_active (GTK_SWITCH (widget), g_settings_get_boolean (settings, "enable-sound-alerts"));
  g_signal_handlers_unblock_by_func (G_OBJECT (widget), sound_alerts_switch_state_set_cb, dialog);
  gtk_widget_set_sensitive (widget, g_settings_get_boolean (settings, "enable"));
}

static void
update_banner_switch (GtkWidget *dialog)
{
  GtkBuilder *builder;
  GSettings  *settings;
  GSettings  *master_settings;
  GtkWidget  *widget;
  gboolean    notifications_enabled;
  gboolean    show_banners;
  gboolean    active;
  gboolean    sensitive;

  builder = GTK_BUILDER (g_object_get_data (G_OBJECT (dialog), "builder"));
  settings = G_SETTINGS (g_object_get_data (G_OBJECT (dialog), "settings"));
  master_settings = G_SETTINGS (g_object_get_data (G_OBJECT (dialog), "master-settings"));

  show_banners = g_settings_get_boolean (master_settings, "show-banners");
  notifications_enabled = g_settings_get_boolean (settings, "enable");

  widget = get_switch (builder, "notification-banners");
  active = g_settings_get_boolean (settings, "show-banners") &&
          show_banners;
  sensitive = notifications_enabled &&
              show_banners;
  g_signal_handlers_block_by_func (G_OBJECT (widget), notification_banners_switch_state_set_cb, dialog);
  gtk_switch_set_active (GTK_SWITCH (widget), active);
  g_signal_handlers_unblock_by_func (G_OBJECT (widget), notification_banners_switch_state_set_cb, dialog);
  gtk_widget_set_sensitive (widget, sensitive);
}

static void
update_banner_content_switch (GtkWidget *dialog)
{
  GtkBuilder *builder;
  GSettings  *settings;
  GSettings  *master_settings;
  GtkWidget  *widget;
  gboolean    notifications_enabled;
  gboolean    show_banners;
  gboolean    active;
  gboolean    sensitive;

  builder = GTK_BUILDER (g_object_get_data (G_OBJECT (dialog), "builder"));
  settings = G_SETTINGS (g_object_get_data (G_OBJECT (dialog), "settings"));
  master_settings = G_SETTINGS (g_object_get_data (G_OBJECT (dialog), "master-settings"));

  show_banners = g_settings_get_boolean (master_settings, "show-banners");
  notifications_enabled = g_settings_get_boolean (settings, "enable");

  widget = get_switch (builder, "notification-banners-content");
  active = g_settings_get_boolean (settings, "force-expanded") &&
           g_settings_get_boolean (settings, "show-banners") &&
           show_banners;
  sensitive = g_settings_get_boolean (settings, "show-banners") &&
              notifications_enabled &&
              show_banners;
  g_signal_handlers_block_by_func (G_OBJECT (widget), notification_banners_content_switch_state_set_cb, dialog);
  gtk_switch_set_active (GTK_SWITCH (widget), active);
  g_signal_handlers_unblock_by_func (G_OBJECT (widget), notification_banners_content_switch_state_set_cb, dialog);
  gtk_widget_set_sensitive (widget, sensitive);
}

static void
update_lock_screen_switch (GtkWidget *dialog)
{
  GtkBuilder *builder;
  GSettings  *settings;
  GSettings  *master_settings;
  GtkWidget  *widget;
  gboolean    notifications_enabled;
  gboolean    show_in_lock_screen;
  gboolean    active;
  gboolean    sensitive;

  builder = GTK_BUILDER (g_object_get_data (G_OBJECT (dialog), "builder"));
  settings = G_SETTINGS (g_object_get_data (G_OBJECT (dialog), "settings"));
  master_settings = G_SETTINGS (g_object_get_data (G_OBJECT (dialog), "master-settings"));

  show_in_lock_screen = g_settings_get_boolean (master_settings, "show-in-lock-screen");
  notifications_enabled = g_settings_get_boolean (settings, "enable");

  widget = get_switch (builder, "lock-screen-notifications");
  active = g_settings_get_boolean (settings, "show-in-lock-screen") &&
           show_in_lock_screen;
  sensitive = notifications_enabled &&
              show_in_lock_screen;

  g_signal_handlers_block_by_func (G_OBJECT (widget), lock_screen_notifications_switch_state_set_cb, dialog);
  gtk_switch_set_active (GTK_SWITCH (widget), active);
  g_signal_handlers_unblock_by_func (G_OBJECT (widget), lock_screen_notifications_switch_state_set_cb, dialog);
  gtk_widget_set_sensitive (widget, sensitive);
}

static void
update_lock_screen_content_switch (GtkWidget *dialog)
{
  GtkBuilder *builder;
  GSettings  *settings;
  GSettings  *master_settings;
  GtkWidget  *widget;
  gboolean    notifications_enabled;
  gboolean    show_in_lock_screen;
  gboolean    active;
  gboolean    sensitive;

  builder = GTK_BUILDER (g_object_get_data (G_OBJECT (dialog), "builder"));
  settings = G_SETTINGS (g_object_get_data (G_OBJECT (dialog), "settings"));
  master_settings = G_SETTINGS (g_object_get_data (G_OBJECT (dialog), "master-settings"));

  show_in_lock_screen = g_settings_get_boolean (master_settings, "show-in-lock-screen");
  notifications_enabled = g_settings_get_boolean (settings, "enable");

  widget = get_switch (builder, "lock-screen-content");
  active = g_settings_get_boolean (settings, "details-in-lock-screen") &&
           g_settings_get_boolean (settings, "show-in-lock-screen") &&
           show_in_lock_screen;
  sensitive = g_settings_get_boolean (settings, "show-in-lock-screen") &&
              notifications_enabled &&
              show_in_lock_screen;
  g_signal_handlers_block_by_func (G_OBJECT (widget), lock_screen_content_switch_state_set_cb, dialog);
  gtk_switch_set_active (GTK_SWITCH (widget), active);
  g_signal_handlers_unblock_by_func (G_OBJECT (widget), lock_screen_content_switch_state_set_cb, dialog);
  gtk_widget_set_sensitive (widget, sensitive);
}

void
cc_build_edit_dialog (CcNotificationsPanel *panel,
                      GAppInfo             *app,
                      GSettings            *settings,
                      GSettings            *master_settings,
                      GDBusProxy           *perm_store)
{
  GtkBuilder *builder;
  GtkWindow  *shell;
  GtkWidget  *dialog;
  GtkWidget  *listbox;
  GError     *error = NULL;
  gchar      *objects[] = { "edit-dialog", NULL };
  guint       builder_result;
  char *app_id;

  builder = gtk_builder_new ();
  builder_result = gtk_builder_add_objects_from_resource (builder,
                                                          "/org/gnome/control-center/notifications/edit-dialog.ui",
                                                          objects,
                                                          &error);

  if (builder_result == 0)
    {
      g_warning ("Could not load ui: %s", error->message);
      g_error_free (error);
      g_object_unref (builder);
      return;
    }

  shell = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (panel)));

  dialog = GTK_WIDGET (gtk_builder_get_object (builder, "edit-dialog"));

  app_id = g_strdup (g_app_info_get_id (app));
  if (g_str_has_suffix (app_id, ".desktop"))
    app_id[strlen (app_id) - strlen (".desktop")] = '\0';
  dialog_set_app_id (dialog, app_id);
  g_free (app_id);

  dialog_set_perm_store (dialog, perm_store);

  g_object_set (dialog,
                "title", g_app_info_get_name (app),
                "transient-for", shell,
                NULL);

  listbox = GTK_WIDGET (gtk_builder_get_object (builder,
                                                "main-listbox"));

  gtk_list_box_set_header_func (GTK_LIST_BOX (listbox),
                                cc_list_box_update_header_func,
                                NULL, NULL);

  /*
   * Store builder, settings and master_settings to the dialog so we can
   * access them from callbacks easily.
   */
  g_object_set_data_full (G_OBJECT (dialog),
                          "builder",
                          builder,
                          g_object_unref);

  g_object_set_data_full (G_OBJECT (dialog),
                          "settings",
                          g_object_ref (settings),
                          g_object_unref);

  g_object_set_data_full (G_OBJECT (dialog),
                          "master-settings",
                          g_object_ref (master_settings),
                          g_object_unref);

  /* Connect signals */
  gtk_builder_add_callback_symbols (builder,
                                    "notifications_switch_state_set_cb",
                                    G_CALLBACK (notifications_switch_state_set_cb),
                                    "sound_alerts_switch_state_set_cb",
                                    G_CALLBACK (sound_alerts_switch_state_set_cb),
                                    "notification_banners_switch_state_set_cb",
                                    G_CALLBACK (notification_banners_switch_state_set_cb),
                                    "notification_banners_content_switch_state_set_cb",
                                    G_CALLBACK (notification_banners_content_switch_state_set_cb),
                                    "lock_screen_notifications_switch_state_set_cb",
                                    G_CALLBACK (lock_screen_notifications_switch_state_set_cb),
                                    "lock_screen_content_switch_state_set_cb",
                                    G_CALLBACK (lock_screen_content_switch_state_set_cb),
                                    NULL);

  gtk_builder_connect_signals (builder, dialog);

  /* Init states of switches */
  update_switches (dialog);

  /* Show the dialog */
  gtk_widget_show_all (dialog);
}
