/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Red Hat, Inc
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

#include "shell/list-box-helper.h"
#include "cc-privacy-panel.h"
#include "cc-privacy-resources.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

CC_PANEL_REGISTER (CcPrivacyPanel, cc_privacy_panel)

#define WID(s) GTK_WIDGET (gtk_builder_get_object (self->priv->builder, s))

#define REMEMBER_RECENT_FILES "remember-recent-files"
#define RECENT_FILES_MAX_AGE "recent-files-max-age"
#define REMOVE_OLD_TRASH_FILES "remove-old-trash-files"
#define REMOVE_OLD_TEMP_FILES "remove-old-temp-files"
#define OLD_FILES_AGE "old-files-age"
#define SEND_SOFTWARE_USAGE_STATS "send-software-usage-stats"
#define LOCATION_ENABLED "enabled"

struct _CcPrivacyPanelPrivate
{
  GtkBuilder *builder;
  GtkWidget  *recent_dialog;
  GtkWidget  *screen_lock_dialog;
  GtkWidget  *trash_dialog;
  GtkWidget  *software_dialog;
  GtkWidget  *list_box;

  GSettings  *lockdown_settings;
  GSettings  *lock_settings;
  GSettings  *privacy_settings;
  GSettings  *notification_settings;
  GSettings  *location_settings;
};

static void
update_lock_screen_sensitivity (CcPrivacyPanel *self)
{
  GtkWidget *widget;
  gboolean   locked;

  locked = g_settings_get_boolean (self->priv->lockdown_settings, "disable-lock-screen");

  widget = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "screen_lock_dialog_grid"));
  gtk_widget_set_sensitive (widget, !locked);
}

static void
on_lockdown_settings_changed (GSettings      *settings,
                              const char     *key,
                              CcPrivacyPanel *panel)
{
  if (g_str_equal (key, "disable-lock-screen") == FALSE)
    return;

  update_lock_screen_sensitivity (panel);
}

static gboolean
on_off_label_mapping_get (GValue   *value,
                          GVariant *variant,
                          gpointer  user_data)
{
  g_value_set_string (value, g_variant_get_boolean (variant) ? _("On") : _("Off"));

  return TRUE;
}

static GtkWidget *
get_on_off_label (GSettings *settings,
                  const gchar *key)
{
  GtkWidget *w;

  w = gtk_label_new ("");
  g_settings_bind_with_mapping (settings, key,
                                w, "label",
                                G_SETTINGS_BIND_GET,
                                on_off_label_mapping_get,
                                NULL,
                                NULL,
                                NULL);
  return w;
}

typedef struct
{
  GtkWidget *label;
  const gchar *key1;
  const gchar *key2;
} Label2Data;

static void
set_on_off_label2 (GSettings   *settings,
                   const gchar *key,
                   gpointer     user_data)
{
  Label2Data *data = user_data;
  gboolean v1, v2;

  v1 = g_settings_get_boolean (settings, data->key1);
  v2 = g_settings_get_boolean (settings, data->key2);

  gtk_label_set_label (GTK_LABEL (data->label), (v1 || v2) ? _("On") : _("Off"));
}

static GtkWidget *
get_on_off_label2 (GSettings *settings,
                   const gchar *key1,
                   const gchar *key2)
{
  Label2Data *data;

  data = g_new (Label2Data, 1);
  data->label = gtk_label_new ("");
  data->key1 = g_strdup (key1);
  data->key2 = g_strdup (key2);

  g_signal_connect (settings, "changed",
                    G_CALLBACK (set_on_off_label2), data);

  set_on_off_label2 (settings, key1, data);

  return data->label;
}

static void
add_row (CcPrivacyPanel *self,
         const gchar    *label,
         const gchar    *dialog_id,
         GtkWidget      *status)
{
  GtkWidget *box, *row, *w;

  row = gtk_list_box_row_new ();
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 50);
  gtk_container_add (GTK_CONTAINER (row), box);
  g_object_set_data (G_OBJECT (row), "dialog-id", (gpointer)dialog_id);
  gtk_widget_set_hexpand (box, TRUE);
  gtk_container_add (GTK_CONTAINER (self->priv->list_box), row);

  w = gtk_label_new (label);
  gtk_misc_set_alignment (GTK_MISC (w), 0.0f, 0.5f);
  gtk_widget_set_margin_start (w, 20);
  gtk_widget_set_margin_end (w, 20);
  gtk_widget_set_margin_top (w, 12);
  gtk_widget_set_margin_bottom (w, 12);
  gtk_widget_set_halign (w, GTK_ALIGN_START);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (w, TRUE);
  gtk_box_pack_start (GTK_BOX (box), w, TRUE, TRUE, 0);
  gtk_widget_set_margin_start (status, 20);
  gtk_widget_set_margin_end (status, 20);
  gtk_widget_set_halign (status, GTK_ALIGN_END);
  gtk_widget_set_valign (status, GTK_ALIGN_CENTER);
  gtk_box_pack_end (GTK_BOX (box), status, FALSE, FALSE, 0);

  gtk_widget_show_all (row);
}

static void
lock_combo_changed_cb (GtkWidget      *widget,
                       CcPrivacyPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  guint delay;
  gboolean ret;

  /* no selection */
  ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX(widget), &iter);
  if (!ret)
    return;

  /* get entry */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX(widget));
  gtk_tree_model_get (model, &iter,
                      1, &delay,
                      -1);
  g_settings_set (self->priv->lock_settings, "lock-delay", "u", delay);
}

static void
set_lock_value_for_combo (GtkComboBox    *combo_box,
                          CcPrivacyPanel *self)
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

  /* try to make the UI match the lock setting */
  g_settings_get (self->priv->lock_settings, "lock-delay", "u", &value);
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

static void
add_screen_lock (CcPrivacyPanel *self)
{
  GtkWidget *w;
  GtkWidget *dialog;
  GtkWidget *label;

  w = get_on_off_label (self->priv->lock_settings, "lock-enabled");
  add_row (self, _("Screen Lock"), "screen_lock_dialog", w);

  dialog = self->priv->screen_lock_dialog;
  g_signal_connect (dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "automatic_screen_lock"));
  g_settings_bind (self->priv->lock_settings, "lock-enabled",
                   w, "active",
                   G_SETTINGS_BIND_DEFAULT);

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "lock_after_combo"));
  g_settings_bind (self->priv->lock_settings, "lock-enabled",
                   w, "sensitive",
                   G_SETTINGS_BIND_GET);

  label = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "lock_after_label"));

  g_object_bind_property (w, "sensitive", label, "sensitive", G_BINDING_DEFAULT);

  set_lock_value_for_combo (GTK_COMBO_BOX (w), self);
  g_signal_connect (w, "changed",
                    G_CALLBACK (lock_combo_changed_cb), self);

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "show_notifications"));
  g_settings_bind (self->priv->notification_settings, "show-in-lock-screen",
                   w, "active",
                   G_SETTINGS_BIND_DEFAULT);
}

static void
retain_history_combo_changed_cb (GtkWidget      *widget,
                                 CcPrivacyPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gint value;
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
  g_settings_set (self->priv->privacy_settings, RECENT_FILES_MAX_AGE, "i", value);
}

static void
set_retain_history_value_for_combo (GtkComboBox    *combo_box,
                                    CcPrivacyPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gint value;
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

  /* try to make the UI match the setting */
  g_settings_get (self->priv->privacy_settings, RECENT_FILES_MAX_AGE, "i", &value);
  do
    {
      gtk_tree_model_get (model, &iter,
                          1, &value_tmp,
                          -1);
      if (value == value_tmp ||
          (value > 0 && value_tmp > value_prev && value < value_tmp))
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

static void
clear_recent (CcPrivacyPanel *self)
{
  GtkRecentManager *m;

  m = gtk_recent_manager_get_default ();
  gtk_recent_manager_purge_items (m, NULL);
}

static void
add_usage_history (CcPrivacyPanel *self)
{
  GtkWidget *w;
  GtkWidget *dialog;
  GtkWidget *label;

  w = get_on_off_label (self->priv->privacy_settings, REMEMBER_RECENT_FILES);
  add_row (self, _("Usage & History"), "recent_dialog", w);

  dialog = self->priv->recent_dialog;
  g_signal_connect (dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "recently_used_switch"));
  g_settings_bind (self->priv->privacy_settings, REMEMBER_RECENT_FILES,
                   w, "active",
                   G_SETTINGS_BIND_DEFAULT);

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "retain_history_combo"));
  set_retain_history_value_for_combo (GTK_COMBO_BOX (w), self);
  g_signal_connect (w, "changed",
                    G_CALLBACK (retain_history_combo_changed_cb), self);

  g_settings_bind (self->priv->privacy_settings, REMEMBER_RECENT_FILES,
                   w, "sensitive",
                   G_SETTINGS_BIND_GET);

  label = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "retain_history_label"));

  g_object_bind_property (w, "sensitive", label, "sensitive", G_BINDING_DEFAULT);
  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "clear_recent_button"));
  g_signal_connect_swapped (w, "clicked",
                            G_CALLBACK (clear_recent), self);

}

static void
purge_after_combo_changed_cb (GtkWidget      *widget,
                              CcPrivacyPanel *self)
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
  g_settings_set (self->priv->privacy_settings, OLD_FILES_AGE, "u", value);
}

static void
set_purge_after_value_for_combo (GtkComboBox    *combo_box,
                                 CcPrivacyPanel *self)
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
  g_settings_get (self->priv->privacy_settings, OLD_FILES_AGE, "u", &value);
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
run_warning (GtkWindow *parent, char *prompt, char *text, char *button_title)
{
  GtkWidget *dialog;
  int result;
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

  result = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  return result == GTK_RESPONSE_OK;
}

static void
empty_trash (CcPrivacyPanel *self)
{
  GDBusConnection *bus;
  gboolean result;
  GtkWidget *dialog;

  dialog = WID ("trash_dialog");
  result = run_warning (GTK_WINDOW (dialog), _("Empty all items from Trash?"),
                        _("All items in the Trash will be permanently deleted."),
                        _("_Empty Trash"));

  if (!result)
    return; 

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_dbus_connection_call (bus,
                          "org.gnome.SettingsDaemon",
                          "/org/gnome/SettingsDaemon/Housekeeping",
                          "org.gnome.SettingsDaemon.Housekeeping",
                          "EmptyTrash",
                          NULL, NULL, 0, -1, NULL, NULL, NULL);
  g_object_unref (bus);
}

static void
purge_temp (CcPrivacyPanel *self)
{
  GDBusConnection *bus;
  gboolean result;
  GtkWidget *dialog;

  dialog = WID ("trash_dialog");
  result = run_warning (GTK_WINDOW (dialog), _("Delete all the temporary files?"),
                        _("All the temporary files will be permanently deleted."),
                        _("_Purge Temporary Files"));

  if (!result)
    return; 

  bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
  g_dbus_connection_call (bus,
                          "org.gnome.SettingsDaemon",
                          "/org/gnome/SettingsDaemon/Housekeeping",
                          "org.gnome.SettingsDaemon.Housekeeping",
                          "RemoveTempFiles",
                          NULL, NULL, 0, -1, NULL, NULL, NULL);
  g_object_unref (bus);
}

static void
add_trash_temp (CcPrivacyPanel *self)
{
  GtkWidget *w;
  GtkWidget *dialog;

  w = get_on_off_label2 (self->priv->privacy_settings, REMOVE_OLD_TRASH_FILES, REMOVE_OLD_TEMP_FILES);
  add_row (self, _("Purge Trash & Temporary Files"), "trash_dialog", w);

  dialog = self->priv->trash_dialog;
  g_signal_connect (dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "purge_trash_switch"));
  g_settings_bind (self->priv->privacy_settings, REMOVE_OLD_TRASH_FILES,
                   w, "active",
                   G_SETTINGS_BIND_DEFAULT);

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "purge_temp_switch"));
  g_settings_bind (self->priv->privacy_settings, REMOVE_OLD_TEMP_FILES,
                   w, "active",
                   G_SETTINGS_BIND_DEFAULT);

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "purge_after_combo"));

  set_purge_after_value_for_combo (GTK_COMBO_BOX (w), self);
  g_signal_connect (w, "changed",
                    G_CALLBACK (purge_after_combo_changed_cb), self);

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "empty_trash_button"));
  g_signal_connect_swapped (w, "clicked", G_CALLBACK (empty_trash), self);

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "purge_temp_button"));
  g_signal_connect_swapped (w, "clicked", G_CALLBACK (purge_temp), self);
}

static void
add_software (CcPrivacyPanel *self)
{
  GtkWidget *w;
  GtkWidget *dialog;

  /* disable until all the points on the bug are fixed:
   * https://bugzilla.gnome.org/show_bug.cgi?id=726234 */
  return;

  w = get_on_off_label (self->priv->privacy_settings, SEND_SOFTWARE_USAGE_STATS);
  add_row (self, _("Software Usage"), "software_dialog", w);

  dialog = self->priv->software_dialog;
  g_signal_connect (dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "software_usage_switch"));
  g_settings_bind (self->priv->privacy_settings, SEND_SOFTWARE_USAGE_STATS,
                   w, "active",
                   G_SETTINGS_BIND_DEFAULT);
}

static void
cc_privacy_panel_finalize (GObject *object)
{
  CcPrivacyPanelPrivate *priv = CC_PRIVACY_PANEL (object)->priv;

  g_clear_pointer (&priv->recent_dialog, gtk_widget_destroy);
  g_clear_pointer (&priv->screen_lock_dialog, gtk_widget_destroy);
  g_clear_pointer (&priv->trash_dialog, gtk_widget_destroy);
  g_clear_pointer (&priv->software_dialog, gtk_widget_destroy);
  g_clear_object (&priv->builder);
  g_clear_object (&priv->lockdown_settings);
  g_clear_object (&priv->lock_settings);
  g_clear_object (&priv->privacy_settings);
  g_clear_object (&priv->notification_settings);
  g_clear_object (&priv->location_settings);

  G_OBJECT_CLASS (cc_privacy_panel_parent_class)->finalize (object);
}

static const char *
cc_privacy_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/privacy";
}

static void
activate_row (CcPrivacyPanel *self,
              GtkListBoxRow  *row)
{
  GObject *w;
  const gchar *dialog_id;
  GtkWidget *toplevel;

  dialog_id = g_object_get_data (G_OBJECT (row), "dialog-id");
  w = gtk_builder_get_object (self->priv->builder, dialog_id);
  if (w == NULL)
    {
      g_warning ("No such dialog: %s", dialog_id);
      return;
    }

  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  gtk_window_set_transient_for (GTK_WINDOW (w), GTK_WINDOW (toplevel));
  gtk_window_set_modal (GTK_WINDOW (w), TRUE);
  gtk_window_present (GTK_WINDOW (w));
}

static void
cc_privacy_panel_init (CcPrivacyPanel *self)
{
  GError    *error;
  GtkWidget *widget;
  GtkWidget *frame;
  guint res;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, CC_TYPE_PRIVACY_PANEL, CcPrivacyPanelPrivate);
  g_resources_register (cc_privacy_get_resource ());

  self->priv->builder = gtk_builder_new ();

  error = NULL;
  res = gtk_builder_add_from_resource (self->priv->builder,
                                       "/org/gnome/control-center/privacy/privacy.ui",
                                       &error);

  if (res == 0)
    {
      g_warning ("Could not load interface file: %s",
                 (error != NULL) ? error->message : "unknown error");
      g_clear_error (&error);
      return;
    }

  self->priv->recent_dialog = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "recent_dialog"));
  self->priv->screen_lock_dialog = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "screen_lock_dialog"));
  self->priv->trash_dialog = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "trash_dialog"));
  self->priv->software_dialog = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "software_dialog"));

  frame = WID ("frame");
  widget = gtk_list_box_new ();
  gtk_list_box_set_selection_mode (GTK_LIST_BOX (widget), GTK_SELECTION_NONE);
  gtk_container_add (GTK_CONTAINER (frame), widget);
  self->priv->list_box = widget;
  gtk_widget_show (widget);

  g_signal_connect_swapped (widget, "row-activated",
                            G_CALLBACK (activate_row), self);

  gtk_list_box_set_header_func (GTK_LIST_BOX (widget),
                                cc_list_box_update_header_func,
                                NULL, NULL);

  self->priv->lockdown_settings = g_settings_new ("org.gnome.desktop.lockdown");
  self->priv->lock_settings = g_settings_new ("org.gnome.desktop.screensaver");
  self->priv->privacy_settings = g_settings_new ("org.gnome.desktop.privacy");
  self->priv->notification_settings = g_settings_new ("org.gnome.desktop.notifications");
  self->priv->location_settings = g_settings_new ("org.gnome.system.location");

  add_screen_lock (self);
  add_usage_history (self);
  add_trash_temp (self);
  add_software (self);

  g_signal_connect (self->priv->lockdown_settings, "changed",
                    G_CALLBACK (on_lockdown_settings_changed), self);
  update_lock_screen_sensitivity (self);

  widget = WID ("location_services_switch");
  gtk_switch_set_active (GTK_SWITCH (widget),
                         g_settings_get_boolean (self->priv->location_settings,
                                                 LOCATION_ENABLED));
  g_settings_bind (self->priv->location_settings,
                   LOCATION_ENABLED,
                   widget, "active",
                   G_SETTINGS_BIND_DEFAULT);

  widget = WID ("privacy_vbox");
  gtk_container_add (GTK_CONTAINER (self), widget);
}

static void
cc_privacy_panel_class_init (CcPrivacyPanelClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_privacy_panel_get_help_uri;

  oclass->finalize = cc_privacy_panel_finalize;

  g_type_class_add_private (klass, sizeof (CcPrivacyPanelPrivate));
}
