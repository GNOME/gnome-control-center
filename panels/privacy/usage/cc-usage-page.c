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

#include "cc-usage-page.h"
#include "cc-number-row.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

struct _CcUsagePage
{
  AdwNavigationPage parent_instance;

  GSettings    *privacy_settings;

  AdwDialog    *clear_file_history_dialog;
  AdwDialog    *delete_temp_files_dialog;
  AdwDialog    *empty_trash_dialog;
  CcNumberRow  *purge_after_row;
  AdwSwitchRow *purge_temp_row;
  AdwSwitchRow *purge_trash_row;
  AdwSwitchRow *recently_used_row;
  CcNumberRow  *retain_history_row;
};

G_DEFINE_TYPE (CcUsagePage, cc_usage_page, ADW_TYPE_NAVIGATION_PAGE)

static void
on_clear_history_response_cb (void)
{
  gtk_recent_manager_purge_items (gtk_recent_manager_get_default (), NULL);
}

static void
on_empty_trash_response_cb (void)
{
  g_autoptr(GDBusConnection) bus = NULL;

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_dbus_connection_call (bus,
                          "org.gnome.SettingsDaemon.Housekeeping",
                          "/org/gnome/SettingsDaemon/Housekeeping",
                          "org.gnome.SettingsDaemon.Housekeeping",
                          "EmptyTrash",
                          NULL, NULL, 0, -1, NULL, NULL, NULL);
}

static void
on_purge_temp_response_cb (void)
{
  g_autoptr(GDBusConnection) bus = NULL;

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_dbus_connection_call (bus,
                          "org.gnome.SettingsDaemon.Housekeeping",
                          "/org/gnome/SettingsDaemon/Housekeeping",
                          "org.gnome.SettingsDaemon.Housekeeping",
                          "RemoveTempFiles",
                          NULL, NULL, 0, -1, NULL, NULL, NULL);
}

static void
cc_usage_page_finalize (GObject *object)
{
  CcUsagePage *self = CC_USAGE_PAGE (object);

  g_clear_object (&self->privacy_settings);

  G_OBJECT_CLASS (cc_usage_page_parent_class)->finalize (object);
}

static void
cc_usage_page_init (CcUsagePage *self)
{
  g_type_ensure (CC_TYPE_NUMBER_ROW);

  gtk_widget_init_template (GTK_WIDGET (self));

  self->privacy_settings = g_settings_new ("org.gnome.desktop.privacy");

  g_settings_bind (self->privacy_settings,
                   "remember-recent-files",
                   self->recently_used_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  cc_number_row_bind_settings (self->retain_history_row,
                               self->privacy_settings,
                               "recent-files-max-age");

  g_settings_bind (self->privacy_settings,
                   "remember-recent-files",
                   self->retain_history_row,
                   "sensitive",
                   G_SETTINGS_BIND_GET);

  g_settings_bind (self->privacy_settings, "remove-old-trash-files",
                   self->purge_trash_row, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->privacy_settings, "remove-old-temp-files",
                   self->purge_temp_row, "active",
                   G_SETTINGS_BIND_DEFAULT);

  cc_number_row_bind_settings (self->purge_after_row,
                               self->privacy_settings,
                               "old-files-age");
}

static void
cc_usage_page_class_init (CcUsagePageClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  oclass->finalize = cc_usage_page_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/privacy/usage/cc-usage-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcUsagePage, clear_file_history_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcUsagePage, delete_temp_files_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcUsagePage, empty_trash_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcUsagePage, purge_after_row);
  gtk_widget_class_bind_template_child (widget_class, CcUsagePage, purge_temp_row);
  gtk_widget_class_bind_template_child (widget_class, CcUsagePage, purge_trash_row);
  gtk_widget_class_bind_template_child (widget_class, CcUsagePage, recently_used_row);
  gtk_widget_class_bind_template_child (widget_class, CcUsagePage, retain_history_row);

  gtk_widget_class_bind_template_callback (widget_class, on_clear_history_response_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_empty_trash_response_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_purge_temp_response_cb);
}
