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

#include "cc-usage-panel.h"
#include "cc-usage-panel-enums.h"
#include "cc-usage-resources.h"
#include "cc-util.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

struct _CcUsagePanel
{
  CcPanel         parent_instance;

  GSettings      *privacy_settings;

  AdwSwitchRow   *recently_used_switch;
  AdwComboRow    *retain_history_combo;

  AdwSwitchRow   *purge_trash_switch;
  AdwSwitchRow   *purge_temp_switch;
  AdwComboRow    *purge_after_combo;
  GtkButton      *purge_temp_button;
  GtkButton      *purge_trash_button;
};

CC_PANEL_REGISTER (CcUsagePanel, cc_usage_panel)

static char *
purge_after_name_cb (AdwEnumListItem *item,
                     gpointer         user_data)
{
  switch (adw_enum_list_item_get_value (item))
    {
    case CC_USAGE_PANEL_PURGE_AFTER_1_HOUR:
      /* Translators: Option for "Automatically Delete Period" in "Trash & Temporary Files" group */
      return g_strdup (C_("purge_files", "1 hour"));
    case CC_USAGE_PANEL_PURGE_AFTER_1_DAY:
      /* Translators: Option for "Automatically Delete Period" in "Trash & Temporary Files" group */
      return g_strdup (C_("purge_files", "1 day"));
    case CC_USAGE_PANEL_PURGE_AFTER_2_DAYS:
      /* Translators: Option for "Automatically Delete Period" in "Trash & Temporary Files" group */
      return g_strdup (C_("purge_files", "2 days"));
    case CC_USAGE_PANEL_PURGE_AFTER_3_DAYS:
      /* Translators: Option for "Automatically Delete Period" in "Trash & Temporary Files" group */
      return g_strdup (C_("purge_files", "3 days"));
    case CC_USAGE_PANEL_PURGE_AFTER_4_DAYS:
      /* Translators: Option for "Automatically Delete Period" in "Trash & Temporary Files" group */
      return g_strdup (C_("purge_files", "4 days"));
    case CC_USAGE_PANEL_PURGE_AFTER_5_DAYS:
      /* Translators: Option for "Automatically Delete Period" in "Trash & Temporary Files" group */
      return g_strdup (C_("purge_files", "5 days"));
    case CC_USAGE_PANEL_PURGE_AFTER_6_DAYS:
      /* Translators: Option for "Automatically Delete Period" in "Trash & Temporary Files" group */
      return g_strdup (C_("purge_files", "6 days"));
    case CC_USAGE_PANEL_PURGE_AFTER_7_DAYS:
      /* Translators: Option for "Automatically Delete Period" in "Trash & Temporary Files" group */
      return g_strdup (C_("purge_files", "7 days"));
    case CC_USAGE_PANEL_PURGE_AFTER_14_DAYS:
      /* Translators: Option for "Automatically Delete Period" in "Trash & Temporary Files" group */
      return g_strdup (C_("purge_files", "14 days"));
    case CC_USAGE_PANEL_PURGE_AFTER_30_DAYS:
      /* Translators: Option for "Automatically Delete Period" in "Trash & Temporary Files" group */
      return g_strdup (C_("purge_files", "30 days"));
    default:
      return NULL;
    }
}

static void
purge_after_combo_changed_cb (AdwComboRow  *combo_row,
                              GParamSpec   *pspec,
                              CcUsagePanel *self)
{
  AdwEnumListItem *item;
  CcUsagePanelPurgeAfter value;

  item = ADW_ENUM_LIST_ITEM (adw_combo_row_get_selected_item (combo_row));
  value = adw_enum_list_item_get_value (item);

  g_settings_set (self->privacy_settings, "old-files-age", "u", value);
}

static void
set_purge_after_value_for_combo (AdwComboRow  *combo_row,
                                 CcUsagePanel *self)
{
  AdwEnumListModel *model;
  guint value;

  model = ADW_ENUM_LIST_MODEL (adw_combo_row_get_model (combo_row));

  g_settings_get (self->privacy_settings, "old-files-age", "u", &value);
  adw_combo_row_set_selected (combo_row,
                              adw_enum_list_model_find_position (model, value));
}

static GtkDialog *
run_warning (CcUsagePanel *self,
             const gchar  *prompt,
             const gchar  *text,
             const gchar  *button_title)
{
  GtkWindow *parent;
  GtkWidget *dialog;
  GtkWidget *button;
  CcShell *shell;

  shell = cc_panel_get_shell (CC_PANEL (self));
  parent = GTK_WINDOW (cc_shell_get_toplevel (shell));

  dialog = gtk_message_dialog_new (parent,
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
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
  gtk_widget_add_css_class (button, "destructive-action");

  gtk_window_present (GTK_WINDOW (dialog));

  return GTK_DIALOG (dialog);
}

static void
on_empty_trash_warning_response_cb (GtkDialog    *dialog,
                                    gint          response,
                                    CcUsagePanel *self)
{
  g_autoptr(GDBusConnection) bus = NULL;

  if (response != GTK_RESPONSE_OK)
    goto out;

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_dbus_connection_call (bus,
                          "org.gnome.SettingsDaemon.Housekeeping",
                          "/org/gnome/SettingsDaemon/Housekeeping",
                          "org.gnome.SettingsDaemon.Housekeeping",
                          "EmptyTrash",
                          NULL, NULL, 0, -1, NULL, NULL, NULL);

out:
  gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
empty_trash (CcUsagePanel *self)
{
  GtkDialog *dialog;

  dialog = run_warning (self,
                        _("Empty all items from Trash?"),
                        _("All items in the Trash will be permanently deleted."),
                        _("_Empty Trash"));

  g_signal_connect_object (dialog,
                           "response",
                           G_CALLBACK (on_empty_trash_warning_response_cb),
                           self,
                           0);
}

static void
on_purge_temp_warning_response_cb (GtkDialog    *dialog,
                                   gint          response,
                                   CcUsagePanel *self)
{
  g_autoptr(GDBusConnection) bus = NULL;

  if (response != GTK_RESPONSE_OK)
    goto out;

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_dbus_connection_call (bus,
                          "org.gnome.SettingsDaemon.Housekeeping",
                          "/org/gnome/SettingsDaemon/Housekeeping",
                          "org.gnome.SettingsDaemon.Housekeeping",
                          "RemoveTempFiles",
                          NULL, NULL, 0, -1, NULL, NULL, NULL);

out:
  gtk_window_destroy (GTK_WINDOW (dialog));
}
static void
purge_temp (CcUsagePanel *self)
{
  GtkDialog *dialog;

  dialog = run_warning (self,
                        _("Delete all the temporary files?"),
                        _("All the temporary files will be permanently deleted."),
                        _("_Purge Temporary Files"));

  g_signal_connect_object (dialog,
                           "response",
                           G_CALLBACK (on_purge_temp_warning_response_cb),
                           self,
                           0);
}

static void
cc_usage_panel_finalize (GObject *object)
{
  CcUsagePanel *self = CC_USAGE_PANEL (object);

  g_clear_object (&self->privacy_settings);

  G_OBJECT_CLASS (cc_usage_panel_parent_class)->finalize (object);
}

static char *
retain_history_name_cb (AdwEnumListItem *item,
                        gpointer         user_data)
{
  switch (adw_enum_list_item_get_value (item))
    {
    case CC_USAGE_PANEL_RETAIN_HISTORY_1_DAY:
      /* Translators: Option for "File History Duration" in "File History" group */
      return g_strdup (C_("retain_history", "1 day"));
    case CC_USAGE_PANEL_RETAIN_HISTORY_7_DAYS:
      /* Translators: Option for "File History Duration" in "File History" group */
      return g_strdup (C_("retain_history", "7 days"));
    case CC_USAGE_PANEL_RETAIN_HISTORY_30_DAYS:
      /* Translators: Option for "File History Duration" in "File History" group */
      return g_strdup (C_("retain_history", "30 days"));
    case CC_USAGE_PANEL_RETAIN_HISTORY_FOREVER:
      /* Translators: Option for "File History Duration" in "File History" group */
      return g_strdup (C_("retain_history", "Forever"));
    default:
      return NULL;
    }
}

static void
retain_history_combo_changed_cb (AdwComboRow  *combo_row,
                                 GParamSpec   *pspec,
                                 CcUsagePanel *self)
{
  AdwEnumListItem *item;
  CcUsagePanelRetainHistory value;

  item = ADW_ENUM_LIST_ITEM (adw_combo_row_get_selected_item (combo_row));
  value = adw_enum_list_item_get_value (item);

  g_settings_set (self->privacy_settings, "recent-files-max-age", "i", value);
}

static void
set_retain_history_value_for_combo (AdwComboRow  *combo_row,
                                    CcUsagePanel *self)
{
  AdwEnumListModel *model;
  gint value;

  model = ADW_ENUM_LIST_MODEL (adw_combo_row_get_model (combo_row));

  g_settings_get (self->privacy_settings, "recent-files-max-age", "i", &value);
  adw_combo_row_set_selected (combo_row,
                              adw_enum_list_model_find_position (model, value));
}

static void
cc_usage_panel_init (CcUsagePanel *self)
{
  g_resources_register (cc_usage_get_resource ());

  g_type_ensure (CC_TYPE_USAGE_PANEL_PURGE_AFTER);
  g_type_ensure (CC_TYPE_USAGE_PANEL_RETAIN_HISTORY);

  gtk_widget_init_template (GTK_WIDGET (self));

  self->privacy_settings = g_settings_new ("org.gnome.desktop.privacy");

  g_settings_bind (self->privacy_settings,
                   "remember-recent-files",
                   self->recently_used_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  set_retain_history_value_for_combo (self->retain_history_combo, self);

  g_settings_bind (self->privacy_settings,
                   "remember-recent-files",
                   self->retain_history_combo,
                   "sensitive",
                   G_SETTINGS_BIND_GET);

  g_settings_bind (self->privacy_settings, "remove-old-trash-files",
                   self->purge_trash_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->privacy_settings, "remove-old-temp-files",
                   self->purge_temp_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  set_purge_after_value_for_combo (self->purge_after_combo, self);

  g_signal_connect_object (self->purge_trash_button, "clicked", G_CALLBACK (empty_trash), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->purge_temp_button, "clicked", G_CALLBACK (purge_temp), self, G_CONNECT_SWAPPED);
}

static void
on_clear_recent_warning_response_cb (GtkDialog    *dialog,
                                   gint          response,
                                   CcUsagePanel *self)
{
  if (response == GTK_RESPONSE_OK)
    gtk_recent_manager_purge_items (gtk_recent_manager_get_default (), NULL);

  gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
clear_recent (CcUsagePanel *self)
{
  GtkDialog *dialog;

  dialog = run_warning (self,
                        _("Clear File History?"),
                        _("After clearing, lists of recently used files will appear empty."),
                        _("_Clear History"));

  g_signal_connect_object (dialog,
                           "response",
                           G_CALLBACK (on_clear_recent_warning_response_cb),
                           self,
                           0);
}

static void
cc_usage_panel_class_init (CcUsagePanelClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  oclass->finalize = cc_usage_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/usage/cc-usage-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcUsagePanel, purge_after_combo);
  gtk_widget_class_bind_template_child (widget_class, CcUsagePanel, purge_temp_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUsagePanel, purge_trash_button);
  gtk_widget_class_bind_template_child (widget_class, CcUsagePanel, purge_trash_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUsagePanel, purge_temp_button);
  gtk_widget_class_bind_template_child (widget_class, CcUsagePanel, recently_used_switch);
  gtk_widget_class_bind_template_child (widget_class, CcUsagePanel, retain_history_combo);

  gtk_widget_class_bind_template_callback (widget_class, clear_recent);
  gtk_widget_class_bind_template_callback (widget_class, retain_history_name_cb);
  gtk_widget_class_bind_template_callback (widget_class, retain_history_combo_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, purge_after_name_cb);
  gtk_widget_class_bind_template_callback (widget_class, purge_after_combo_changed_cb);
}
