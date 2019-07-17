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
#include "cc-app-notifications-dialog.h"

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

static void update_banner_switch (CcAppNotificationsDialog *dialog);
static void update_banner_content_switch (CcAppNotificationsDialog *dialog);
static void update_lock_screen_switch (CcAppNotificationsDialog *dialog);
static void update_lock_screen_content_switch (CcAppNotificationsDialog *dialog);
static void update_sound_switch (CcAppNotificationsDialog *dialog);
static void update_notification_switch (CcAppNotificationsDialog *dialog);

struct _CcAppNotificationsDialog {
  HdyDialog            parent;

  GSettings           *settings;
  GSettings           *master_settings;
  gchar               *app_id;
  GDBusProxy          *perm_store;

  GtkWidget           *main_listbox;
  GtkWidget           *notifications_switch;
  GtkWidget           *sound_alerts_switch;
  GtkWidget           *notification_banners_switch;
  GtkWidget           *notification_banners_content_switch;
  GtkWidget           *lock_screen_notifications_switch;
  GtkWidget           *lock_screen_content_switch;
};

G_DEFINE_TYPE (CcAppNotificationsDialog, cc_app_notifications_dialog, HDY_TYPE_DIALOG)

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
set_portal_permissions_for_app (CcAppNotificationsDialog *dialog, GtkSwitch *the_switch)
{
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

  if (dialog->perm_store == NULL)
    {
      g_warning ("Could not find PermissionStore, not syncing notification permissions");
      return;
    }

  new_perms = g_variant_new_strv (allow ? yes_strv : no_strv, 1);
  g_variant_ref_sink (new_perms);

  g_variant_builder_init (&builder, G_VARIANT_TYPE("a{sas}"));
  found = FALSE;

  reply = g_dbus_proxy_call_sync (dialog->perm_store,
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
          if (g_strcmp0 (key, dialog->app_id) == 0)
            {
              found = TRUE;
              g_variant_builder_add (&builder, "{s@as}", key, new_perms);
            }
          else
            g_variant_builder_add (&builder, "{s@as}", key, value);
        }
    }

  if (!found)
    g_variant_builder_add (&builder, "{s@as}", dialog->app_id, new_perms);

  g_dbus_proxy_call (dialog->perm_store,
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
notifications_switch_state_set_cb (GtkSwitch    *widget,
                                   GParamSpec   *pspec,
                                   CcAppNotificationsDialog *dialog)
{
  g_settings_set_boolean (dialog->settings, "enable", gtk_switch_get_active (widget));
  set_portal_permissions_for_app (dialog, widget);
  update_sound_switch (dialog);
  update_banner_switch (dialog);
  update_banner_content_switch (dialog);
  update_lock_screen_switch (dialog);
  update_lock_screen_content_switch (dialog);
}

static void
sound_alerts_switch_state_set_cb (GtkSwitch    *widget,
                                  GParamSpec   *pspec,
                                  CcAppNotificationsDialog *dialog)
{
  g_settings_set_boolean (dialog->settings, "enable-sound-alerts", gtk_switch_get_active (widget));
}

static void
notification_banners_switch_state_set_cb (GtkSwitch    *widget,
                                          GParamSpec   *pspec,
                                          CcAppNotificationsDialog *dialog)
{
  g_settings_set_boolean (dialog->settings, "show-banners", gtk_switch_get_active (widget));
  update_banner_content_switch (dialog);
}

static void
notification_banners_content_switch_state_set_cb (GtkSwitch    *widget,
                                                  GParamSpec   *pspec,
                                                  CcAppNotificationsDialog *dialog)
{
  g_settings_set_boolean (dialog->settings, "force-expanded", gtk_switch_get_active (widget));
}

static void
lock_screen_notifications_switch_state_set_cb (GtkSwitch    *widget,
                                               GParamSpec   *pspec,
                                               CcAppNotificationsDialog *dialog)
{
  g_settings_set_boolean (dialog->settings, "show-in-lock-screen", gtk_switch_get_active (widget));
  update_lock_screen_content_switch (dialog);
}

static void
lock_screen_content_switch_state_set_cb (GtkSwitch    *widget,
                                         GParamSpec   *pspec,
                                         CcAppNotificationsDialog *dialog)
{
  g_settings_set_boolean (dialog->settings, "details-in-lock-screen", gtk_switch_get_active (widget));
}

static void
update_switches (CcAppNotificationsDialog *dialog)
{
  update_notification_switch (dialog);
  update_sound_switch (dialog);
  update_banner_switch (dialog);
  update_banner_content_switch (dialog);
  update_lock_screen_switch (dialog);
  update_lock_screen_content_switch (dialog);
}

static void
update_notification_switch (CcAppNotificationsDialog *dialog)
{
  g_signal_handlers_block_by_func (G_OBJECT (dialog->notifications_switch), notifications_switch_state_set_cb, dialog);
  gtk_switch_set_active (GTK_SWITCH (dialog->notifications_switch), g_settings_get_boolean (dialog->settings, "enable"));
  g_signal_handlers_unblock_by_func (G_OBJECT (dialog->notifications_switch), notifications_switch_state_set_cb, dialog);
}

static void
update_sound_switch (CcAppNotificationsDialog *dialog)
{
  g_signal_handlers_block_by_func (G_OBJECT (dialog->sound_alerts_switch), sound_alerts_switch_state_set_cb, dialog);
  gtk_switch_set_active (GTK_SWITCH (dialog->sound_alerts_switch), g_settings_get_boolean (dialog->settings, "enable-sound-alerts"));
  g_signal_handlers_unblock_by_func (G_OBJECT (dialog->sound_alerts_switch), sound_alerts_switch_state_set_cb, dialog);
  gtk_widget_set_sensitive (dialog->sound_alerts_switch, g_settings_get_boolean (dialog->settings, "enable"));
}

static void
update_banner_switch (CcAppNotificationsDialog *dialog)
{
  gboolean notifications_enabled;
  gboolean show_banners;
  gboolean active;
  gboolean sensitive;

  show_banners = g_settings_get_boolean (dialog->master_settings, "show-banners");
  notifications_enabled = g_settings_get_boolean (dialog->settings, "enable");

  active = g_settings_get_boolean (dialog->settings, "show-banners") &&
          show_banners;
  sensitive = notifications_enabled &&
              show_banners;
  g_signal_handlers_block_by_func (G_OBJECT (dialog->notification_banners_switch), notification_banners_switch_state_set_cb, dialog);
  gtk_switch_set_active (GTK_SWITCH (dialog->notification_banners_switch), active);
  g_signal_handlers_unblock_by_func (G_OBJECT (dialog->notification_banners_switch), notification_banners_switch_state_set_cb, dialog);
  gtk_widget_set_sensitive (dialog->notification_banners_switch, sensitive);
}

static void
update_banner_content_switch (CcAppNotificationsDialog *dialog)
{
  gboolean notifications_enabled;
  gboolean show_banners;
  gboolean active;
  gboolean sensitive;

  show_banners = g_settings_get_boolean (dialog->master_settings, "show-banners");
  notifications_enabled = g_settings_get_boolean (dialog->settings, "enable");

  active = g_settings_get_boolean (dialog->settings, "force-expanded") &&
           g_settings_get_boolean (dialog->settings, "show-banners") &&
           show_banners;
  sensitive = g_settings_get_boolean (dialog->settings, "show-banners") &&
              notifications_enabled &&
              show_banners;
  g_signal_handlers_block_by_func (G_OBJECT (dialog->notification_banners_content_switch), notification_banners_content_switch_state_set_cb, dialog);
  gtk_switch_set_active (GTK_SWITCH (dialog->notification_banners_content_switch), active);
  g_signal_handlers_unblock_by_func (G_OBJECT (dialog->notification_banners_content_switch), notification_banners_content_switch_state_set_cb, dialog);
  gtk_widget_set_sensitive (dialog->notification_banners_content_switch, sensitive);
}

static void
update_lock_screen_switch (CcAppNotificationsDialog *dialog)
{
  gboolean notifications_enabled;
  gboolean show_in_lock_screen;
  gboolean active;
  gboolean sensitive;

  show_in_lock_screen = g_settings_get_boolean (dialog->master_settings, "show-in-lock-screen");
  notifications_enabled = g_settings_get_boolean (dialog->settings, "enable");

  active = g_settings_get_boolean (dialog->settings, "show-in-lock-screen") &&
           show_in_lock_screen;
  sensitive = notifications_enabled &&
              show_in_lock_screen;

  g_signal_handlers_block_by_func (G_OBJECT (dialog->lock_screen_notifications_switch), lock_screen_notifications_switch_state_set_cb, dialog);
  gtk_switch_set_active (GTK_SWITCH (dialog->lock_screen_notifications_switch), active);
  g_signal_handlers_unblock_by_func (G_OBJECT (dialog->lock_screen_notifications_switch), lock_screen_notifications_switch_state_set_cb, dialog);
  gtk_widget_set_sensitive (dialog->lock_screen_notifications_switch, sensitive);
}

static void
update_lock_screen_content_switch (CcAppNotificationsDialog *dialog)
{
  gboolean notifications_enabled;
  gboolean show_in_lock_screen;
  gboolean active;
  gboolean sensitive;

  show_in_lock_screen = g_settings_get_boolean (dialog->master_settings, "show-in-lock-screen");
  notifications_enabled = g_settings_get_boolean (dialog->settings, "enable");

  active = g_settings_get_boolean (dialog->settings, "details-in-lock-screen") &&
           g_settings_get_boolean (dialog->settings, "show-in-lock-screen") &&
           show_in_lock_screen;
  sensitive = g_settings_get_boolean (dialog->settings, "show-in-lock-screen") &&
              notifications_enabled &&
              show_in_lock_screen;
  g_signal_handlers_block_by_func (G_OBJECT (dialog->lock_screen_content_switch), lock_screen_content_switch_state_set_cb, dialog);
  gtk_switch_set_active (GTK_SWITCH (dialog->lock_screen_content_switch), active);
  g_signal_handlers_unblock_by_func (G_OBJECT (dialog->lock_screen_content_switch), lock_screen_content_switch_state_set_cb, dialog);
  gtk_widget_set_sensitive (dialog->lock_screen_content_switch, sensitive);
}

static void
cc_app_notifications_dialog_dispose (GObject *object)
{
  CcAppNotificationsDialog *dialog = CC_APP_NOTIFICATIONS_DIALOG (object);

  g_clear_object (&dialog->settings);
  g_clear_object (&dialog->master_settings);
  g_clear_pointer (&dialog->app_id, g_free);
  g_clear_object (&dialog->perm_store);

  G_OBJECT_CLASS (cc_app_notifications_dialog_parent_class)->dispose (object);
}

static void
cc_app_notifications_dialog_class_init (CcAppNotificationsDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_app_notifications_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/notifications/cc-app-notifications-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcAppNotificationsDialog, main_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcAppNotificationsDialog, notifications_switch);
  gtk_widget_class_bind_template_child (widget_class, CcAppNotificationsDialog, sound_alerts_switch);
  gtk_widget_class_bind_template_child (widget_class, CcAppNotificationsDialog, notification_banners_switch);
  gtk_widget_class_bind_template_child (widget_class, CcAppNotificationsDialog, notification_banners_content_switch);
  gtk_widget_class_bind_template_child (widget_class, CcAppNotificationsDialog, lock_screen_notifications_switch);
  gtk_widget_class_bind_template_child (widget_class, CcAppNotificationsDialog, lock_screen_content_switch);

  gtk_widget_class_bind_template_callback (widget_class, notifications_switch_state_set_cb);
  gtk_widget_class_bind_template_callback (widget_class, sound_alerts_switch_state_set_cb);
  gtk_widget_class_bind_template_callback (widget_class, notification_banners_switch_state_set_cb);
  gtk_widget_class_bind_template_callback (widget_class, notification_banners_content_switch_state_set_cb);
  gtk_widget_class_bind_template_callback (widget_class, lock_screen_notifications_switch_state_set_cb);
  gtk_widget_class_bind_template_callback (widget_class, lock_screen_content_switch_state_set_cb);
}

void
cc_app_notifications_dialog_init (CcAppNotificationsDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));

  gtk_list_box_set_header_func (GTK_LIST_BOX (dialog->main_listbox),
                                cc_list_box_update_header_func,
                                NULL, NULL);
}

CcAppNotificationsDialog *
cc_app_notifications_dialog_new (const gchar          *app_id,
                                 const gchar          *title,
                                 GSettings            *settings,
                                 GSettings            *master_settings,
                                 GDBusProxy           *perm_store)
{
  CcAppNotificationsDialog *dialog;

  dialog = g_object_new (CC_TYPE_APP_NOTIFICATIONS_DIALOG,
                         "use-header-bar", 1,
                         NULL);

  gtk_window_set_title (GTK_WINDOW (dialog), title);
  dialog->settings = g_object_ref (settings);
  dialog->master_settings = g_object_ref (master_settings);
  dialog->app_id = g_strdup (app_id);
  dialog->perm_store = g_object_ref (perm_store);

  update_switches (dialog);

  return dialog;
}
