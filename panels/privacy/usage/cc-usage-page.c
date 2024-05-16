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
#include "cc-usage-page-enums.h"
#include "cc-util.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

struct _CcUsagePage
{
  AdwNavigationPage parent_instance;

  GSettings    *privacy_settings;

  AdwDialog    *clear_file_history_dialog;
  AdwDialog    *delete_temp_files_dialog;
  AdwDialog    *empty_trash_dialog;
  AdwComboRow  *purge_after_combo;
  AdwSwitchRow *purge_temp_row;
  AdwSwitchRow *purge_trash_row;
  AdwSwitchRow *recently_used_row;
  AdwComboRow  *retain_history_combo;
};

G_DEFINE_TYPE (CcUsagePage, cc_usage_page, ADW_TYPE_NAVIGATION_PAGE)

static char *
purge_after_name_cb (AdwEnumListItem *item,
                     gpointer         user_data)
{
  switch (adw_enum_list_item_get_value (item))
    {
    case CC_USAGE_PAGE_PURGE_AFTER_1_HOUR:
      /* Translators: Option for "Automatically Delete Period" in "Trash & Temporary Files" group */
      return g_strdup (C_("purge_files", "1 hour"));
    case CC_USAGE_PAGE_PURGE_AFTER_1_DAY:
      /* Translators: Option for "Automatically Delete Period" in "Trash & Temporary Files" group */
      return g_strdup (C_("purge_files", "1 day"));
    case CC_USAGE_PAGE_PURGE_AFTER_2_DAYS:
      /* Translators: Option for "Automatically Delete Period" in "Trash & Temporary Files" group */
      return g_strdup (C_("purge_files", "2 days"));
    case CC_USAGE_PAGE_PURGE_AFTER_3_DAYS:
      /* Translators: Option for "Automatically Delete Period" in "Trash & Temporary Files" group */
      return g_strdup (C_("purge_files", "3 days"));
    case CC_USAGE_PAGE_PURGE_AFTER_4_DAYS:
      /* Translators: Option for "Automatically Delete Period" in "Trash & Temporary Files" group */
      return g_strdup (C_("purge_files", "4 days"));
    case CC_USAGE_PAGE_PURGE_AFTER_5_DAYS:
      /* Translators: Option for "Automatically Delete Period" in "Trash & Temporary Files" group */
      return g_strdup (C_("purge_files", "5 days"));
    case CC_USAGE_PAGE_PURGE_AFTER_6_DAYS:
      /* Translators: Option for "Automatically Delete Period" in "Trash & Temporary Files" group */
      return g_strdup (C_("purge_files", "6 days"));
    case CC_USAGE_PAGE_PURGE_AFTER_7_DAYS:
      /* Translators: Option for "Automatically Delete Period" in "Trash & Temporary Files" group */
      return g_strdup (C_("purge_files", "7 days"));
    case CC_USAGE_PAGE_PURGE_AFTER_14_DAYS:
      /* Translators: Option for "Automatically Delete Period" in "Trash & Temporary Files" group */
      return g_strdup (C_("purge_files", "14 days"));
    case CC_USAGE_PAGE_PURGE_AFTER_30_DAYS:
      /* Translators: Option for "Automatically Delete Period" in "Trash & Temporary Files" group */
      return g_strdup (C_("purge_files", "30 days"));
    default:
      return NULL;
    }
}

static void
purge_after_combo_changed_cb (AdwComboRow *combo_row,
                              GParamSpec  *pspec,
                              CcUsagePage *self)
{
  AdwEnumListItem *item;
  CcUsagePagePurgeAfter value;

  item = ADW_ENUM_LIST_ITEM (adw_combo_row_get_selected_item (combo_row));
  value = adw_enum_list_item_get_value (item);

  g_settings_set (self->privacy_settings, "old-files-age", "u", value);
}

static void
set_purge_after_value_for_combo (AdwComboRow *combo_row,
                                 CcUsagePage *self)
{
  AdwEnumListModel *model;
  guint value;

  model = ADW_ENUM_LIST_MODEL (adw_combo_row_get_model (combo_row));

  g_settings_get (self->privacy_settings, "old-files-age", "u", &value);
  adw_combo_row_set_selected (combo_row,
                              adw_enum_list_model_find_position (model, value));
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

static char *
retain_history_name_cb (AdwEnumListItem *item,
                        gpointer         user_data)
{
  switch (adw_enum_list_item_get_value (item))
    {
    case CC_USAGE_PAGE_RETAIN_HISTORY_1_DAY:
      /* Translators: Option for "File History Duration" in "File History" group */
      return g_strdup (C_("retain_history", "1 day"));
    case CC_USAGE_PAGE_RETAIN_HISTORY_7_DAYS:
      /* Translators: Option for "File History Duration" in "File History" group */
      return g_strdup (C_("retain_history", "7 days"));
    case CC_USAGE_PAGE_RETAIN_HISTORY_30_DAYS:
      /* Translators: Option for "File History Duration" in "File History" group */
      return g_strdup (C_("retain_history", "30 days"));
    case CC_USAGE_PAGE_RETAIN_HISTORY_FOREVER:
      /* Translators: Option for "File History Duration" in "File History" group */
      return g_strdup (C_("retain_history", "Forever"));
    default:
      return NULL;
    }
}

static void
retain_history_combo_changed_cb (AdwComboRow *combo_row,
                                 GParamSpec  *pspec,
                                 CcUsagePage *self)
{
  AdwEnumListItem *item;
  CcUsagePageRetainHistory value;

  item = ADW_ENUM_LIST_ITEM (adw_combo_row_get_selected_item (combo_row));
  value = adw_enum_list_item_get_value (item);

  g_settings_set (self->privacy_settings, "recent-files-max-age", "i", value);
}

static void
set_retain_history_value_for_combo (AdwComboRow *combo_row,
                                    CcUsagePage *self)
{
  AdwEnumListModel *model;
  gint value;

  model = ADW_ENUM_LIST_MODEL (adw_combo_row_get_model (combo_row));

  g_settings_get (self->privacy_settings, "recent-files-max-age", "i", &value);
  adw_combo_row_set_selected (combo_row,
                              adw_enum_list_model_find_position (model, value));
}

static void
cc_usage_page_init (CcUsagePage *self)
{
  g_type_ensure (CC_TYPE_USAGE_PAGE_PURGE_AFTER);
  g_type_ensure (CC_TYPE_USAGE_PAGE_RETAIN_HISTORY);

  gtk_widget_init_template (GTK_WIDGET (self));

  self->privacy_settings = g_settings_new ("org.gnome.desktop.privacy");

  g_settings_bind (self->privacy_settings,
                   "remember-recent-files",
                   self->recently_used_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  set_retain_history_value_for_combo (self->retain_history_combo, self);

  g_settings_bind (self->privacy_settings,
                   "remember-recent-files",
                   self->retain_history_combo,
                   "sensitive",
                   G_SETTINGS_BIND_GET);

  g_settings_bind (self->privacy_settings, "remove-old-trash-files",
                   self->purge_trash_row, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->privacy_settings, "remove-old-temp-files",
                   self->purge_temp_row, "active",
                   G_SETTINGS_BIND_DEFAULT);

  set_purge_after_value_for_combo (self->purge_after_combo, self);
}

static void
on_clear_history_response_cb (void)
{
  gtk_recent_manager_purge_items (gtk_recent_manager_get_default (), NULL);
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
  gtk_widget_class_bind_template_child (widget_class, CcUsagePage, purge_after_combo);
  gtk_widget_class_bind_template_child (widget_class, CcUsagePage, purge_temp_row);
  gtk_widget_class_bind_template_child (widget_class, CcUsagePage, purge_trash_row);
  gtk_widget_class_bind_template_child (widget_class, CcUsagePage, recently_used_row);
  gtk_widget_class_bind_template_child (widget_class, CcUsagePage, retain_history_combo);


  gtk_widget_class_bind_template_callback (widget_class, on_clear_history_response_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_empty_trash_response_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_purge_temp_response_cb);
  gtk_widget_class_bind_template_callback (widget_class, retain_history_combo_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, retain_history_name_cb);
  gtk_widget_class_bind_template_callback (widget_class, purge_after_combo_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, purge_after_name_cb);
}
