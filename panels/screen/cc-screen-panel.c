/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2018 Red Hat, Inc
 * Copyright (C) 2020 Collabora Ltd.
 * Copyright (C) 2021-2022 Canonical Ltd.
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

#include "cc-screen-panel.h"
#include "cc-screen-panel-enums.h"
#include "cc-screen-resources.h"
#include "cc-util.h"

#include "panels/display/cc-display-config-manager-dbus.h"

#include <adwaita.h>

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

struct _CcScreenPanel
{
  CcPanel        parent_instance;

  CcDisplayConfigManager *display_config_manager;

  GSettings     *lock_settings;
  GSettings     *notification_settings;
  GSettings     *privacy_settings;
  GSettings     *session_settings;

  GCancellable  *cancellable;

  AdwComboRow         *blank_screen_row;
  AdwComboRow         *lock_after_row;
  AdwPreferencesGroup *screen_privacy_group;
  GDBusProxy          *usb_proxy;
  AdwSwitchRow        *automatic_screen_lock_switch;
  AdwSwitchRow        *privacy_screen_switch;
  AdwSwitchRow        *show_notifications_switch;
  AdwSwitchRow        *usb_protection_switch;
};

CC_PANEL_REGISTER (CcScreenPanel, cc_screen_panel)

static char *
lock_after_name_cb (AdwEnumListItem *item,
                    gpointer         user_data)
{

  switch (adw_enum_list_item_get_value (item))
    {
    case CC_SCREEN_PANEL_LOCK_AFTER_SCREEN_OFF:
      /* Translators: Option for "Lock screen after blank" in "Screen Lock" panel */
      return g_strdup(C_("lock_screen", "Screen Turns Off"));
    case CC_SCREEN_PANEL_LOCK_AFTER_30_SEC:
      /* Translators: Option for "Lock screen after blank" in "Screen Lock" panel */
      return g_strdup (C_("lock_screen", "30 seconds"));
    case CC_SCREEN_PANEL_LOCK_AFTER_1_MIN:
      /* Translators: Option for "Lock screen after blank" in "Screen Lock" panel */
      return g_strdup (C_("lock_screen", "1 minute"));
    case CC_SCREEN_PANEL_LOCK_AFTER_2_MIN:
      /* Translators: Option for "Lock screen after blank" in "Screen Lock" panel */
      return g_strdup (C_("lock_screen", "2 minutes"));
    case CC_SCREEN_PANEL_LOCK_AFTER_3_MIN:
      /* Translators: Option for "Lock screen after blank" in "Screen Lock" panel */
      return g_strdup (C_("lock_screen", "3 minutes"));
    case CC_SCREEN_PANEL_LOCK_AFTER_5_MIN:
      /* Translators: Option for "Lock screen after blank" in "Screen Lock" panel */
      return g_strdup (C_("lock_screen", "5 minutes"));
    case CC_SCREEN_PANEL_LOCK_AFTER_30_MIN:
      /* Translators: Option for "Lock screen after blank" in "Screen Lock" panel */
      return g_strdup (C_("lock_screen", "30 minutes"));
    case CC_SCREEN_PANEL_LOCK_AFTER_1_HR:
      /* Translators: Option for "Lock screen after blank" in "Screen Lock" panel */
      return g_strdup (C_("lock_screen", "1 hour"));
    default:
      return NULL;
    }
}

static void
on_lock_combo_changed_cb (AdwComboRow   *combo_row,
                          GParamSpec    *pspec,
                          CcScreenPanel *self)
{
  AdwEnumListItem *item;
  CcScreenPanelLockAfter delay;

  item = ADW_ENUM_LIST_ITEM (adw_combo_row_get_selected_item (combo_row));
  delay = adw_enum_list_item_get_value (item);

  g_settings_set (self->lock_settings, "lock-delay", "u", delay);
}

static void
set_lock_value_for_combo (AdwComboRow   *combo_row,
                          CcScreenPanel *self)
{
  AdwEnumListModel *model;
  guint value;

  model = ADW_ENUM_LIST_MODEL (adw_combo_row_get_model (combo_row));

  g_settings_get (self->lock_settings, "lock-delay", "u", &value);
  adw_combo_row_set_selected (combo_row,
                              adw_enum_list_model_find_position (model, value));
}

static char *
screen_delay_name_cb (AdwEnumListItem *item,
                      gpointer         user_data)
{

  switch (adw_enum_list_item_get_value (item))
    {
    case CC_SCREEN_PANEL_BLANK_SCREEN_DELAY_1_MIN:
      /* Translators: Option for "Blank screen" in "Screen Lock" panel */
      return g_strdup (C_("blank_screen", "1 minute"));
    case CC_SCREEN_PANEL_BLANK_SCREEN_DELAY_2_MIN:
      /* Translators: Option for "Blank screen" in "Screen Lock" panel */
      return g_strdup (C_("blank_screen", "2 minutes"));
    case CC_SCREEN_PANEL_BLANK_SCREEN_DELAY_3_MIN:
      /* Translators: Option for "Blank screen" in "Screen Lock" panel */
      return g_strdup (C_("blank_screen", "3 minutes"));
    case CC_SCREEN_PANEL_BLANK_SCREEN_DELAY_4_MIN:
      /* Translators: Option for "Blank screen" in "Screen Lock" panel */
      return g_strdup (C_("blank_screen", "4 minutes"));
    case CC_SCREEN_PANEL_BLANK_SCREEN_DELAY_5_MIN:
      /* Translators: Option for "Blank screen" in "Screen Lock" panel */
      return g_strdup (C_("blank_screen", "5 minutes"));
    case CC_SCREEN_PANEL_BLANK_SCREEN_DELAY_8_MIN:
      /* Translators: Option for "Blank screen" in "Screen Lock" panel */
      return g_strdup (C_("blank_screen", "8 minutes"));
    case CC_SCREEN_PANEL_BLANK_SCREEN_DELAY_10_MIN:
      /* Translators: Option for "Blank screen" in "Screen Lock" panel */
      return g_strdup (C_("blank_screen", "10 minutes"));
    case CC_SCREEN_PANEL_BLANK_SCREEN_DELAY_12_MIN:
      /* Translators: Option for "Blank screen" in "Screen Lock" panel */
      return g_strdup (C_("blank_screen", "12 minutes"));
    case CC_SCREEN_PANEL_BLANK_SCREEN_DELAY_15_MIN:
      /* Translators: Option for "Blank screen" in "Screen Lock" panel */
      return g_strdup (C_("blank_screen", "15 minutes"));
    case CC_SCREEN_PANEL_BLANK_SCREEN_DELAY_NEVER:
      /* Translators: Option for "Blank screen" in "Screen Lock" panel */
      return g_strdup (C_("blank_screen", "Never"));
    default:
      return NULL;
    }
}

static void
set_blank_screen_delay_value (CcScreenPanel *self,
                              gint           value)
{
  AdwEnumListModel *model;

  model = ADW_ENUM_LIST_MODEL (adw_combo_row_get_model (self->blank_screen_row));

  adw_combo_row_set_selected (self->blank_screen_row,
                              adw_enum_list_model_find_position (model, value));
}

static void
on_blank_screen_delay_changed_cb (AdwComboRow   *combo_row,
                                  GParamSpec    *pspec,
                                  CcScreenPanel *self)
{
  AdwEnumListItem *item;
  CcScreenPanelBlankScreenDelay delay;

  item = ADW_ENUM_LIST_ITEM (adw_combo_row_get_selected_item (combo_row));
  delay = adw_enum_list_item_get_value (item);

  g_settings_set_uint (self->session_settings, "idle-delay", delay);
}

static void
on_usb_protection_properties_changed_cb (GDBusProxy    *usb_proxy,
                                         GVariant      *changed_properties,
                                         GStrv          invalidated_properties,
                                         CcScreenPanel *self)
{
  gboolean available = FALSE;

  if (self->usb_proxy)
    {
      g_autoptr(GVariant) variant = NULL;

      variant = g_dbus_proxy_get_cached_property (self->usb_proxy, "Available");
      if (variant != NULL)
        available = g_variant_get_boolean (variant);
    }

  /* Show the USB protection row only if the required daemon is up and running */
  gtk_widget_set_visible (GTK_WIDGET (self->usb_protection_switch), available);
}

static void
on_usb_protection_param_ready (GObject      *source_object,
                               GAsyncResult *res,
                               gpointer      user_data)
{
  g_autoptr(GError) error = NULL;
  CcScreenPanel *self;
  GDBusProxy *proxy;

  self = user_data;
  proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        {
          g_warning ("Failed to connect to SettingsDaemon.UsbProtection: %s",
                     error->message);
        }

      gtk_widget_set_visible (GTK_WIDGET (self->usb_protection_switch), FALSE);
      return;
    }
  self->usb_proxy = proxy;

  g_signal_connect_object (self->usb_proxy,
                           "g-properties-changed",
                           G_CALLBACK (on_usb_protection_properties_changed_cb),
                           self,
                           0);
  on_usb_protection_properties_changed_cb (self->usb_proxy, NULL, NULL, self);
}

static void
cc_screen_panel_finalize (GObject *object)
{
  CcScreenPanel *self = CC_SCREEN_PANEL (object);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_object (&self->display_config_manager);
  g_clear_object (&self->lock_settings);
  g_clear_object (&self->notification_settings);
  g_clear_object (&self->session_settings);
  g_clear_object (&self->usb_proxy);

  G_OBJECT_CLASS (cc_screen_panel_parent_class)->finalize (object);
}

static void
cc_screen_panel_class_init (CcScreenPanelClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  oclass->finalize = cc_screen_panel_finalize;

  g_type_ensure (CC_TYPE_SCREEN_PANEL_LOCK_AFTER);
  g_type_ensure (CC_TYPE_SCREEN_PANEL_BLANK_SCREEN_DELAY);
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/screen/cc-screen-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcScreenPanel, automatic_screen_lock_switch);
  gtk_widget_class_bind_template_child (widget_class, CcScreenPanel, blank_screen_row);
  gtk_widget_class_bind_template_child (widget_class, CcScreenPanel, lock_after_row);
  gtk_widget_class_bind_template_child (widget_class, CcScreenPanel, privacy_screen_switch);
  gtk_widget_class_bind_template_child (widget_class, CcScreenPanel, screen_privacy_group);
  gtk_widget_class_bind_template_child (widget_class, CcScreenPanel, show_notifications_switch);
  gtk_widget_class_bind_template_child (widget_class, CcScreenPanel, usb_protection_switch);

  gtk_widget_class_bind_template_callback (widget_class, lock_after_name_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_blank_screen_delay_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_lock_combo_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, screen_delay_name_cb);
}

static void
update_display_config (CcScreenPanel *self)
{
  g_autoptr (CcDisplayConfig) config = NULL;
  gboolean any_privacy_screen = FALSE;
  gboolean any_configurable_privacy_screen = FALSE;
  GList *monitors;
  GList *l;

  config = cc_display_config_manager_get_current (self->display_config_manager);
  monitors = config ? cc_display_config_get_monitors (config) : NULL;

  for (l = monitors; l; l = l->next)
    {
      CcDisplayMonitor *monitor = CC_DISPLAY_MONITOR (l->data);
      CcDisplayMonitorPrivacy privacy = cc_display_monitor_get_privacy (monitor);

      if (privacy != CC_DISPLAY_MONITOR_PRIVACY_UNSUPPORTED)
        {
          any_privacy_screen = TRUE;

          if (!(privacy & CC_DISPLAY_MONITOR_PRIVACY_LOCKED))
            any_configurable_privacy_screen = TRUE;
        }
    }

  gtk_widget_set_visible (GTK_WIDGET (self->screen_privacy_group),
                          any_privacy_screen);
  gtk_widget_set_sensitive (GTK_WIDGET (self->privacy_screen_switch),
                            any_configurable_privacy_screen);
}

static void
cc_screen_panel_init (CcScreenPanel *self)
{
  guint value;

  g_resources_register (cc_screen_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();

  self->lock_settings = g_settings_new ("org.gnome.desktop.screensaver");
  self->privacy_settings = g_settings_new ("org.gnome.desktop.privacy");
  self->notification_settings = g_settings_new ("org.gnome.desktop.notifications");
  self->session_settings = g_settings_new ("org.gnome.desktop.session");

  g_settings_bind (self->lock_settings,
                   "lock-enabled",
                   self->automatic_screen_lock_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->lock_settings,
                   "lock-enabled",
                   self->lock_after_row,
                   "sensitive",
                   G_SETTINGS_BIND_GET);

  set_lock_value_for_combo (self->lock_after_row, self);

  g_settings_bind (self->notification_settings,
                   "show-in-lock-screen",
                   self->show_notifications_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  value = g_settings_get_uint (self->session_settings, "idle-delay");
  set_blank_screen_delay_value (self, value);

  g_settings_bind (self->privacy_settings,
                   "usb-protection",
                   self->usb_protection_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  self->display_config_manager = cc_display_config_manager_dbus_new ();
  g_signal_connect_object (self->display_config_manager, "changed",
                           G_CALLBACK (update_display_config), self,
                           G_CONNECT_SWAPPED);

  update_display_config (self);
  g_settings_bind (self->privacy_settings,
                   "privacy-screen",
                   self->privacy_screen_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.gnome.SettingsDaemon.UsbProtection",
                            "/org/gnome/SettingsDaemon/UsbProtection",
                            "org.gnome.SettingsDaemon.UsbProtection",
                            self->cancellable,
                            on_usb_protection_param_ready,
                            self);
}
