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

#include "cc-notifications-panel.h"
#include "cc-app-notifications-page.h"

/*
 *  Key                       Switch
 *
 * "enable",                 "notifications-switch"                 When set to off, all other switches in the page are insensitive
 * "enable-sound-alerts",    "sound-alerts-switch"
 * "show-banners",           "notification-banners-switch"          Off and insensitive when corresponding panel switch is off
 * "force-expanded",         "notification-banners-content-switch"  Off and insensitive when switch above is off
 * "show-in-lock-screen",    "lock-screen-notifications-switch"     Off and insensitive when corresponding panel switch is off
 * "details-in-lock-screen", "lock-screen-content-switch"           Off and insensitive when switch above is off
 */

static void update_banner_row (CcAppNotificationsPage *self);
static void update_banner_content_row (CcAppNotificationsPage *self);
static void update_lock_screen_row (CcAppNotificationsPage *self);
static void update_lock_screen_content_row (CcAppNotificationsPage *self);
static void update_sound_row (CcAppNotificationsPage *self);
static void update_notification_row (CcAppNotificationsPage *self);

struct _CcAppNotificationsPage {
  AdwNavigationPage    parent;

  GSettings           *settings;
  GSettings           *master_settings;
  gchar               *app_id;
  GDBusProxy          *perm_store;

  AdwSwitchRow        *notifications_row;
  AdwSwitchRow        *sound_alerts_row;
  AdwSwitchRow        *notification_banners_row;
  AdwSwitchRow        *notification_banners_content_row;
  AdwSwitchRow        *lock_screen_notifications_row;
  AdwSwitchRow        *lock_screen_content_row;

  gulong notifications_page_change_signal_handler_id;
};

G_DEFINE_TYPE (CcAppNotificationsPage, cc_app_notifications_page, ADW_TYPE_NAVIGATION_PAGE)

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
set_portal_permissions_for_app (CcAppNotificationsPage *self, AdwSwitchRow *row)
{
  gboolean allow = adw_switch_row_get_active (row);
  g_autoptr(GVariant) perms = NULL;
  g_autoptr(GVariant) new_perms = NULL;
  g_autoptr(GVariant) data = NULL;
  GVariantBuilder builder;
  gboolean found;
  int i;
  const char *yes_strv[] = { "yes", NULL };
  const char *no_strv[] = { "no", NULL };
  g_autoptr(GVariant) reply = NULL;

  if (self->perm_store == NULL)
    {
      g_warning ("Could not find PermissionStore, not syncing notification permissions");
      return;
    }

  new_perms = g_variant_new_strv (allow ? yes_strv : no_strv, 1);
  g_variant_ref_sink (new_perms);

  g_variant_builder_init (&builder, G_VARIANT_TYPE("a{sas}"));
  found = FALSE;

  reply = g_dbus_proxy_call_sync (self->perm_store,
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
          if (g_strcmp0 (key, self->app_id) == 0)
            {
              found = TRUE;
              g_variant_builder_add (&builder, "{s@as}", key, new_perms);
            }
          else
            g_variant_builder_add (&builder, "{s@as}", key, value);
        }
    }

  if (!found)
    g_variant_builder_add (&builder, "{s@as}", self->app_id, new_perms);

  g_dbus_proxy_call (self->perm_store,
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
notifications_row_state_set_cb (CcAppNotificationsPage *self)
{
  g_settings_set_boolean (self->settings, "enable", adw_switch_row_get_active (self->notifications_row));
  set_portal_permissions_for_app (self, self->notifications_row);
  update_sound_row (self);
  update_banner_row (self);
  update_banner_content_row (self);
  update_lock_screen_row (self);
  update_lock_screen_content_row (self);
}

static void
sound_alerts_row_state_set_cb (CcAppNotificationsPage *self)
{
  g_settings_set_boolean (self->settings, "enable-sound-alerts", adw_switch_row_get_active (self->sound_alerts_row));
}

static void
notification_banners_row_state_set_cb (CcAppNotificationsPage *self)
{
  g_settings_set_boolean (self->settings, "show-banners", adw_switch_row_get_active (self->notification_banners_row));
  update_banner_content_row (self);
}

static void
notification_banners_content_row_state_set_cb (CcAppNotificationsPage *self)
{
  g_settings_set_boolean (self->settings, "force-expanded", adw_switch_row_get_active (self->notification_banners_content_row));
}

static void
lock_screen_notifications_row_state_set_cb (CcAppNotificationsPage *self)
{
  g_settings_set_boolean (self->settings, "show-in-lock-screen", adw_switch_row_get_active (self->lock_screen_notifications_row));
  update_lock_screen_content_row (self);
}

static void
lock_screen_content_row_state_set_cb (CcAppNotificationsPage *self)
{
  g_settings_set_boolean (self->settings, "details-in-lock-screen", adw_switch_row_get_active (self->lock_screen_notifications_row));
}

static void
update_switches (CcAppNotificationsPage *self)
{
  update_notification_row (self);
  update_sound_row (self);
  update_banner_row (self);
  update_banner_content_row (self);
  update_lock_screen_row (self);
  update_lock_screen_content_row (self);
}

static void
update_notification_row (CcAppNotificationsPage *self)
{
  g_signal_handlers_block_by_func (G_OBJECT (self->notifications_row), notifications_row_state_set_cb, self);
  adw_switch_row_set_active (self->notifications_row, g_settings_get_boolean (self->settings, "enable"));
  g_signal_handlers_unblock_by_func (G_OBJECT (self->notifications_row), notifications_row_state_set_cb, self);
}

static void
update_sound_row (CcAppNotificationsPage *self)
{
  g_signal_handlers_block_by_func (G_OBJECT (self->sound_alerts_row), sound_alerts_row_state_set_cb, self);
  adw_switch_row_set_active (self->sound_alerts_row, g_settings_get_boolean (self->settings, "enable-sound-alerts"));
  g_signal_handlers_unblock_by_func (G_OBJECT (self->sound_alerts_row), sound_alerts_row_state_set_cb, self);
  gtk_widget_set_sensitive (GTK_WIDGET (self->sound_alerts_row), g_settings_get_boolean (self->settings, "enable"));
}

static void
update_banner_row (CcAppNotificationsPage *self)
{
  gboolean notifications_enabled;
  gboolean show_banners;
  gboolean active;
  gboolean sensitive;

  show_banners = g_settings_get_boolean (self->master_settings, "show-banners");
  notifications_enabled = g_settings_get_boolean (self->settings, "enable");

  active = g_settings_get_boolean (self->settings, "show-banners") &&
          show_banners;
  sensitive = notifications_enabled &&
              show_banners;
  g_signal_handlers_block_by_func (G_OBJECT (self->notification_banners_row), notification_banners_row_state_set_cb, self);
  adw_switch_row_set_active (self->notification_banners_row, active);
  g_signal_handlers_unblock_by_func (G_OBJECT (self->notification_banners_row), notification_banners_row_state_set_cb, self);
  gtk_widget_set_sensitive (GTK_WIDGET (self->notification_banners_row), sensitive);
}

static void
update_banner_content_row (CcAppNotificationsPage *self)
{
  gboolean notifications_enabled;
  gboolean show_banners;
  gboolean active;
  gboolean sensitive;

  show_banners = g_settings_get_boolean (self->master_settings, "show-banners");
  notifications_enabled = g_settings_get_boolean (self->settings, "enable");

  active = g_settings_get_boolean (self->settings, "force-expanded") &&
           g_settings_get_boolean (self->settings, "show-banners") &&
           show_banners;
  sensitive = g_settings_get_boolean (self->settings, "show-banners") &&
              notifications_enabled &&
              show_banners;
  g_signal_handlers_block_by_func (G_OBJECT (self->notification_banners_content_row), notification_banners_content_row_state_set_cb, self);
  adw_switch_row_set_active (self->notification_banners_content_row, active);
  g_signal_handlers_unblock_by_func (G_OBJECT (self->notification_banners_content_row), notification_banners_content_row_state_set_cb, self);
  gtk_widget_set_sensitive (GTK_WIDGET (self->notification_banners_content_row), sensitive);
}

static void
update_lock_screen_row (CcAppNotificationsPage *self)
{
  gboolean notifications_enabled;
  gboolean show_in_lock_screen;
  gboolean active;
  gboolean sensitive;

  show_in_lock_screen = g_settings_get_boolean (self->master_settings, "show-in-lock-screen");
  notifications_enabled = g_settings_get_boolean (self->settings, "enable");

  active = g_settings_get_boolean (self->settings, "show-in-lock-screen") &&
           show_in_lock_screen;
  sensitive = notifications_enabled &&
              show_in_lock_screen;

  g_signal_handlers_block_by_func (G_OBJECT (self->lock_screen_notifications_row), lock_screen_notifications_row_state_set_cb, self);
  adw_switch_row_set_active (self->lock_screen_notifications_row, active);
  g_signal_handlers_unblock_by_func (G_OBJECT (self->lock_screen_notifications_row), lock_screen_notifications_row_state_set_cb, self);
  gtk_widget_set_sensitive (GTK_WIDGET (self->lock_screen_notifications_row), sensitive);
}

static void
update_lock_screen_content_row (CcAppNotificationsPage *self)
{
  gboolean notifications_enabled;
  gboolean show_in_lock_screen;
  gboolean active;
  gboolean sensitive;

  show_in_lock_screen = g_settings_get_boolean (self->master_settings, "show-in-lock-screen");
  notifications_enabled = g_settings_get_boolean (self->settings, "enable");

  active = g_settings_get_boolean (self->settings, "details-in-lock-screen") &&
           g_settings_get_boolean (self->settings, "show-in-lock-screen") &&
           show_in_lock_screen;
  sensitive = g_settings_get_boolean (self->settings, "show-in-lock-screen") &&
              notifications_enabled &&
              show_in_lock_screen;
  g_signal_handlers_block_by_func (G_OBJECT (self->lock_screen_content_row), lock_screen_content_row_state_set_cb, self);
  adw_switch_row_set_active (self->lock_screen_content_row, active);
  g_signal_handlers_unblock_by_func (G_OBJECT (self->lock_screen_content_row), lock_screen_content_row_state_set_cb, self);
  gtk_widget_set_sensitive (GTK_WIDGET (self->lock_screen_content_row), sensitive);
}

static void
cc_app_notifications_page_dispose (GObject *object)
{
  CcAppNotificationsPage *self = CC_APP_NOTIFICATIONS_PAGE (object);

  g_clear_signal_handler (&self->notifications_page_change_signal_handler_id, self->settings);

  g_clear_object (&self->settings);
  g_clear_object (&self->master_settings);
  g_clear_pointer (&self->app_id, g_free);
  g_clear_object (&self->perm_store);

  G_OBJECT_CLASS (cc_app_notifications_page_parent_class)->dispose (object);
}

static void
cc_app_notifications_page_class_init (CcAppNotificationsPageClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_app_notifications_page_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/notifications/cc-app-notifications-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcAppNotificationsPage, notifications_row);
  gtk_widget_class_bind_template_child (widget_class, CcAppNotificationsPage, sound_alerts_row);
  gtk_widget_class_bind_template_child (widget_class, CcAppNotificationsPage, notification_banners_row);
  gtk_widget_class_bind_template_child (widget_class, CcAppNotificationsPage, notification_banners_content_row);
  gtk_widget_class_bind_template_child (widget_class, CcAppNotificationsPage, lock_screen_notifications_row);
  gtk_widget_class_bind_template_child (widget_class, CcAppNotificationsPage, lock_screen_content_row);

  gtk_widget_class_bind_template_callback (widget_class, notifications_row_state_set_cb);
  gtk_widget_class_bind_template_callback (widget_class, sound_alerts_row_state_set_cb);
  gtk_widget_class_bind_template_callback (widget_class, notification_banners_row_state_set_cb);
  gtk_widget_class_bind_template_callback (widget_class, notification_banners_content_row_state_set_cb);
  gtk_widget_class_bind_template_callback (widget_class, lock_screen_notifications_row_state_set_cb);
  gtk_widget_class_bind_template_callback (widget_class, lock_screen_content_row_state_set_cb);
}

void
cc_app_notifications_page_init (CcAppNotificationsPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CcAppNotificationsPage *
cc_app_notifications_page_new (const gchar          *app_id,
                               const gchar          *title,
                               GSettings            *settings,
                               GSettings            *master_settings,
                               GDBusProxy           *perm_store)
{
  CcAppNotificationsPage *self;

  self = g_object_new (CC_TYPE_APP_NOTIFICATIONS_PAGE, NULL);

  adw_navigation_page_set_title (ADW_NAVIGATION_PAGE (self), title);
  self->settings = g_object_ref (settings);
  self->master_settings = g_object_ref (master_settings);
  self->app_id = g_strdup (app_id);
  self->perm_store = g_object_ref (perm_store);

  self->notifications_page_change_signal_handler_id =
      g_signal_connect_swapped (self->settings, "changed", G_CALLBACK (update_switches), self);

  update_switches (self);

  return self;
}
