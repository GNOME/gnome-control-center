/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2023 Cyber Phantom <inam123451@gmail.com>
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

#include "cc-internet-resources.h"

#include "cc-internet-panel.h"

#include "shell/cc-object-storage.h"

struct _CcInternetPanel
{
  CcPanel           parent;

  /* RFKill (Airplane Mode) */
  GDBusProxy       *rfkill_proxy;
  AdwSwitchRow     *rfkill_row;
  GtkWidget        *rfkill_widget;
};

CC_PANEL_REGISTER (CcInternetPanel, cc_internet_panel)

static void
rfkill_switch_notify_activate_cb (CcInternetPanel *self)
{
  gboolean enable;

  enable = adw_switch_row_get_active (self->rfkill_row);

  g_dbus_proxy_call (self->rfkill_proxy,
                     "org.freedesktop.DBus.Properties.Set",
                     g_variant_new_parsed ("('org.gnome.SettingsDaemon.Rfkill',"
                                           "'AirplaneMode', %v)",
                                           g_variant_new_boolean (enable)),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     cc_panel_get_cancellable (CC_PANEL (self)),
                     NULL,
                     NULL);
}

static inline gboolean
get_cached_rfkill_property (CcInternetPanel *self,
                            const gchar     *property)
{
  g_autoptr(GVariant) result = NULL;

  result = g_dbus_proxy_get_cached_property (self->rfkill_proxy, property);
  return result ? g_variant_get_boolean (result) : FALSE;
}

static void
sync_airplane_mode_switch (CcInternetPanel *self)
{
  gboolean enabled, should_show, hw_enabled;

  enabled = get_cached_rfkill_property (self, "HasAirplaneMode");
  should_show = get_cached_rfkill_property (self, "ShouldShowAirplaneMode");

  gtk_widget_set_visible (GTK_WIDGET (self->rfkill_widget), enabled && should_show);
  if (!enabled || !should_show)
    return;

  enabled = get_cached_rfkill_property (self, "AirplaneMode");
  hw_enabled = get_cached_rfkill_property (self, "HardwareAirplaneMode");

  enabled |= hw_enabled;

  if (enabled != adw_switch_row_get_active (self->rfkill_row))
    {
      g_signal_handlers_block_by_func (self->rfkill_row,
                                       rfkill_switch_notify_activate_cb,
                                       self);
      g_object_set (self->rfkill_row, "active", enabled, NULL);
      g_signal_handlers_unblock_by_func (self->rfkill_row,
                                         rfkill_switch_notify_activate_cb,
                                         self);
  }

  gtk_widget_set_sensitive (GTK_WIDGET (self->rfkill_row), !hw_enabled);
}

static void
on_rfkill_proxy_properties_changed_cb (CcInternetPanel *self)
{
  g_debug ("Rfkill properties changed");

  sync_airplane_mode_switch (self);
}

static void
rfkill_proxy_acquired_cb (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  CcInternetPanel *self;
  GDBusProxy *proxy;
  g_autoptr(GError) error = NULL;

  proxy = cc_object_storage_create_dbus_proxy_finish (res, &error);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_printerr ("Error creating rfkill proxy: %s\n", error->message);

      return;
    }

  self = CC_INTERNET_PANEL (user_data);
  self->rfkill_proxy = proxy;

  g_signal_connect_object (proxy,
                           "g-properties-changed",
                           G_CALLBACK (on_rfkill_proxy_properties_changed_cb),
                           self,
                           G_CONNECT_SWAPPED);

  sync_airplane_mode_switch (self);
}

static void
cc_internet_panel_dispose (GObject *object)
{
  G_OBJECT_CLASS (cc_internet_panel_parent_class)->dispose (object);
}

static void
cc_internet_panel_finalize (GObject *object)
{
  G_OBJECT_CLASS (cc_internet_panel_parent_class)->finalize (object);
}

static const char *
cc_internet_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/internet";
}

static void
cc_internet_panel_class_init (CcInternetPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_internet_panel_get_help_uri;

  object_class->dispose = cc_internet_panel_dispose;
  object_class->finalize = cc_internet_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/internet/cc-internet-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcInternetPanel, rfkill_row);
  gtk_widget_class_bind_template_child (widget_class, CcInternetPanel, rfkill_widget);

  gtk_widget_class_bind_template_callback (widget_class, rfkill_switch_notify_activate_cb);
}

static void
cc_internet_panel_init (CcInternetPanel *self)
{
  g_resources_register (cc_internet_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  /* Acquire Airplane Mode proxy */
  cc_object_storage_create_dbus_proxy (G_BUS_TYPE_SESSION,
                                       G_DBUS_PROXY_FLAGS_NONE,
                                       "org.gnome.SettingsDaemon.Rfkill",
                                       "/org/gnome/SettingsDaemon/Rfkill",
                                       "org.gnome.SettingsDaemon.Rfkill",
                                       cc_panel_get_cancellable (CC_PANEL (self)),
                                       rfkill_proxy_acquired_cb,
                                       self);
}
