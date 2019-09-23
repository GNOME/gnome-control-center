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

#include "list-box-helper.h"
#include "cc-trash-panel.h"
#include "cc-trash-resources.h"
#include "cc-util.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

struct _CcTrashPanel
{
  CcPanel     parent_instance;

  GSettings  *privacy_settings;

  GtkListBox  *trash_list_box;
  GtkSwitch   *purge_trash_switch;
  GtkSwitch   *purge_temp_switch;
  GtkComboBox *purge_after_combo;
  GtkButton   *purge_temp_button;
  GtkButton   *purge_trash_button;
};

CC_PANEL_REGISTER (CcTrashPanel, cc_trash_panel)

static void
cc_trash_panel_finalize (GObject *object)
{
  CcTrashPanel *self = CC_TRASH_PANEL (object);

  g_clear_object (&self->privacy_settings);

  G_OBJECT_CLASS (cc_trash_panel_parent_class)->finalize (object);
}

static void
purge_after_combo_changed_cb (GtkWidget      *widget,
                              CcTrashPanel *self)
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
  g_settings_set (self->privacy_settings, "old-files-age", "u", value);
}

static void
set_purge_after_value_for_combo (GtkComboBox    *combo_box,
                                 CcTrashPanel *self)
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
  g_settings_get (self->privacy_settings, "old-files-age", "u", &value);
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
run_warning (CcTrashPanel *self, char *prompt, char *text, char *button_title)
{
  GtkWindow *parent;
  GtkWidget *dialog;
  GtkWidget *button;
  int result;

  parent = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self)));

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
empty_trash (CcTrashPanel *self)
{
  GDBusConnection *bus;
  gboolean result;

  result = run_warning (self, _("Empty all items from Trash?"),
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
purge_temp (CcTrashPanel *self)
{
  GDBusConnection *bus;
  gboolean result;

  result = run_warning (self, _("Delete all the temporary files?"),
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
cc_trash_panel_init (CcTrashPanel *self)
{
  g_resources_register (cc_trash_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (self->trash_list_box,
                                cc_list_box_update_header_func,
                                NULL, NULL);

  self->privacy_settings = g_settings_new ("org.gnome.desktop.privacy");

  g_settings_bind (self->privacy_settings, "remove-old-trash-files",
                   self->purge_trash_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->privacy_settings, "remove-old-temp-files",
                   self->purge_temp_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  set_purge_after_value_for_combo (self->purge_after_combo, self);
  g_signal_connect (self->purge_after_combo, "changed",
                    G_CALLBACK (purge_after_combo_changed_cb), self);

  g_signal_connect_swapped (self->purge_trash_button, "clicked", G_CALLBACK (empty_trash), self);

  g_signal_connect_swapped (self->purge_temp_button, "clicked", G_CALLBACK (purge_temp), self);
}

static void
cc_trash_panel_class_init (CcTrashPanelClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  oclass->finalize = cc_trash_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/trash/cc-trash-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcTrashPanel, trash_list_box);
  gtk_widget_class_bind_template_child (widget_class, CcTrashPanel, purge_trash_switch);
  gtk_widget_class_bind_template_child (widget_class, CcTrashPanel, purge_temp_switch);
  gtk_widget_class_bind_template_child (widget_class, CcTrashPanel, purge_after_combo);
  gtk_widget_class_bind_template_child (widget_class, CcTrashPanel, purge_trash_button);
  gtk_widget_class_bind_template_child (widget_class, CcTrashPanel, purge_temp_button);
}
