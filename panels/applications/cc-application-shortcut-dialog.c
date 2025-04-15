/* cc-global-shortcut-dialog.c
 *
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2016 Endless, Inc
 * Copyright (C) 2020 System76, Inc.
 * Copyright (C) 2022 Purism SPC
 * Copyright © 2024 GNOME Foundation Inc.
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
 * Author: Thomas Wood <thomas.wood@intel.com>
 *         Georges Basile Stavracas Neto <gbsneto@gnome.org>
 *         Ian Douglas Scott <idscott@system76.com>
 *         Mohammed Sadiq <sadiq@sadiqpk.org>
 *         Dorota Czaplejewicz <gnome@dorotac.eu>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <adwaita.h>
#include <config.h>
#include <glib/gi18n.h>

#include "cc-application-shortcut-dialog.h"
#include "cc-global-shortcuts-rebind-generated.h"
#include "cc-keyboard-shortcut-group.h"
#include "cc-util.h"

struct _CcApplicationShortcutDialog
{
  AdwDialog parent_instance;

  AdwPreferencesPage *shortcut_list;
  GtkSizeGroup *accelerator_size_group;

  /* Contains `CcKeyboardItem`s */
  GListStore *shortcuts;
  AdwPreferencesGroup *group;

  CcKeyboardManager *manager;

  const char *app_id;
};

G_DEFINE_TYPE (CcApplicationShortcutDialog,
               cc_application_shortcut_dialog,
               ADW_TYPE_DIALOG)

static void
populate_shortcuts_model (CcApplicationShortcutDialog *self,
                          const char                  *section_id,
                          const char                  *section_title)
{
  GtkWidget *group;

  self->shortcuts = g_list_store_new (CC_TYPE_KEYBOARD_ITEM);
  group = cc_keyboard_shortcut_group_new (G_LIST_MODEL (self->shortcuts),
                                          section_id, NULL,
                                          self->manager,
                                          self->accelerator_size_group);
  self->group = ADW_PREFERENCES_GROUP (group);
}

static void
shortcut_changed_cb (CcApplicationShortcutDialog *self)
{
  GVariant *shortcuts;
  g_autoptr(GError) error = NULL;
  g_autoptr(CcGlobalShortcutsRebind) proxy = NULL;

  cc_keyboard_manager_store_global_shortcuts (self->manager, self->app_id);
  shortcuts = cc_keyboard_manager_get_global_shortcuts (self->manager, self->app_id);
  proxy =
    cc_global_shortcuts_rebind_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                       G_DBUS_PROXY_FLAGS_NONE,
                                                       "org.freedesktop.impl.portal.desktop.gnome",
                                                       "/org/gnome/globalshortcuts",
                                                       NULL,
                                                       &error);
  if (!proxy)
    {
      g_warning ("Can't connect to Global Shortcuts Rebind service: %s",
                 error->message);
      return;
    }

  cc_global_shortcuts_rebind_call_rebind_shortcuts_sync (proxy,
                                                         self->app_id,
                                                         shortcuts,
                                                         NULL,
                                                         &error);
  if (error)
    {
      g_warning ("Can't connect to Global Shortcuts Rebind service: %s",
                 error->message);
    }
}

static void
shortcut_added_cb (CcApplicationShortcutDialog *self,
                   CcKeyboardItem              *item,
                   const char                  *section_id,
                   const char                  *section_title)
{
  if (strcmp (section_id, self->app_id) != 0)
    return;

  g_list_store_append (self->shortcuts, item);
  g_signal_connect_object (item,
                           "notify::key-combos",
                           G_CALLBACK (shortcut_changed_cb),
                           self, G_CONNECT_SWAPPED);
}

static void
on_remove_dialog_response_cb (CcApplicationShortcutDialog *self)
{
  cc_keyboard_manager_reset_global_shortcuts (self->manager, self->app_id);
  adw_dialog_close (ADW_DIALOG (self));
}

static void
reset_all_activated_cb (CcApplicationShortcutDialog *self)
{
  AdwAlertDialog *dialog;

  dialog = ADW_ALERT_DIALOG (adw_alert_dialog_new (_("Remove All Shortcuts?"),
                                                   NULL));

  /* TRANSLATORS: %s is an app name. */
  adw_alert_dialog_format_body (dialog,
                                _ ("All actions from %s that have been registered for global shortcuts will be removed."),
                                cc_util_app_id_to_display_name (self->app_id));

  adw_alert_dialog_add_responses (dialog,
                                  "cancel", _("_Cancel"),
                                  "remove", _("_Remove"),
                                  NULL);

  adw_alert_dialog_set_response_appearance (dialog,
                                            "remove",
                                            ADW_RESPONSE_DESTRUCTIVE);

  adw_alert_dialog_set_default_response (dialog,
                                         "cancel");

  adw_alert_dialog_set_close_response (dialog,
                                       "cancel");

  g_signal_connect_swapped (GTK_WIDGET (dialog),
                            "response::remove",
                            G_CALLBACK (on_remove_dialog_response_cb),
                            self);

  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (self));
}

static void
shortcuts_loaded_cb (CcApplicationShortcutDialog *self)
{
  GtkWidget *group, *reset_button = NULL;

  adw_preferences_page_add (self->shortcut_list, ADW_PREFERENCES_GROUP (self->group));

  group = adw_preferences_group_new ();
  reset_button = g_object_new (ADW_TYPE_BUTTON_ROW,
                               "title", _("_Remove All Shortcuts"),
                               "use-underline", TRUE,
                               NULL);
  g_signal_connect_object (reset_button, "activated",
                           G_CALLBACK (reset_all_activated_cb),
                           self,
                           G_CONNECT_SWAPPED);
  gtk_widget_add_css_class (reset_button, "destructive-action");

  adw_preferences_group_add (ADW_PREFERENCES_GROUP (group), reset_button);
  adw_preferences_page_add (self->shortcut_list, ADW_PREFERENCES_GROUP (group));
}

static void
cc_application_shortcut_dialog_finalize (GObject *object)
{
  CcApplicationShortcutDialog *self = CC_APPLICATION_SHORTCUT_DIALOG (object);

  g_clear_object (&self->manager);
  g_clear_object (&self->shortcut_list);
  g_clear_object (&self->accelerator_size_group);

  G_OBJECT_CLASS (cc_application_shortcut_dialog_parent_class)->finalize (object);
}

static void
cc_application_shortcut_dialog_class_init (CcApplicationShortcutDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_application_shortcut_dialog_finalize;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/applications/"
                                               "cc-application-shortcut-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcApplicationShortcutDialog, shortcut_list);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationShortcutDialog, accelerator_size_group);
}

static void
cc_application_shortcut_dialog_init (CcApplicationShortcutDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->manager = cc_keyboard_manager_new ();

  g_signal_connect_object (self->manager,
                           "shortcut-added",
                           G_CALLBACK (shortcut_added_cb),
                           self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->manager,
                           "shortcut-changed",
                           G_CALLBACK (shortcut_changed_cb),
                           self, G_CONNECT_SWAPPED);
  /* Shortcuts can not get individually removed from this dialog,
   * they get rejected in batches */
  g_signal_connect_object (self->manager,
                           "shortcuts-loaded",
                           G_CALLBACK (shortcuts_loaded_cb),
                           self, G_CONNECT_SWAPPED);
}

CcApplicationShortcutDialog *
cc_application_shortcut_dialog_new (const char *app_id)
{
  CcApplicationShortcutDialog *dialog;
  g_autofree char *explanation_str = NULL;
  g_autofree char *formatted_name = NULL;
  g_autofree char *name = NULL;

  dialog = g_object_new (CC_TYPE_APPLICATION_SHORTCUT_DIALOG, NULL);
  dialog->app_id = app_id;
  populate_shortcuts_model (dialog, app_id, app_id);
  cc_keyboard_manager_load_shortcuts (dialog->manager);

  name = cc_util_app_id_to_display_name (app_id);

  formatted_name = g_strdup_printf ("<b>%s</b>", name);
  /* TRANSLATORS: %s is an app name. */
  explanation_str =
    g_strdup_printf (_("%s has registered the following global shortcuts"), formatted_name);
  adw_preferences_page_set_description (dialog->shortcut_list, explanation_str);

  return dialog;
}
