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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Author: Matthias Clasen <mclasen@redhat.com>
 */

#include "cc-privacy-panel.h"
#include "cc-privacy-resources.h"

#include <egg-list-box/egg-list-box.h>
#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

CC_PANEL_REGISTER (CcPrivacyPanel, cc_privacy_panel)

#define WID(s) GTK_WIDGET (gtk_builder_get_object (self->priv->builder, s))

#define REMEMBER_RECENT_FILES "remember-recent-files"
#define RECENT_FILES_MAX_AGE "recent-files-max-age"
#define REMOVE_OLD_TRASH_FILES "remove-old-trash-files"
#define REMOVE_OLD_TEMP_FILES "remove-old-temp-files"
#define OLD_FILES_AGE "old-files-age"

struct _CcPrivacyPanelPrivate
{
  GtkBuilder *builder;
  GtkWidget  *list_box;

  GSettings  *lockdown_settings;
  GSettings  *lock_settings;
  GSettings  *privacy_settings;
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

static gboolean
visible_label_mapping_get (GValue   *value,
                           GVariant *variant,
                           gpointer  user_data)
{
  g_value_set_string (value, g_variant_get_boolean (variant) ? _("Hidden") : _("Visible"));

  return TRUE;
}

static GtkWidget *
get_visible_label (GSettings *settings,
                   const gchar *key)
{
  GtkWidget *w;

  w = gtk_label_new ("");
  g_settings_bind_with_mapping (settings, key,
                                w, "label",
                                G_SETTINGS_BIND_GET,
                                visible_label_mapping_get,
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
  GtkWidget *box, *w;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 50);
  g_object_set_data (G_OBJECT (box), "dialog-id", (gpointer)dialog_id);
  gtk_widget_set_hexpand (box, TRUE);
  gtk_container_add (GTK_CONTAINER (self->priv->list_box), box);

  w = gtk_label_new (label);
  gtk_misc_set_alignment (GTK_MISC (w), 0.0f, 0.5f);
  gtk_widget_set_margin_left (w, 20);
  gtk_widget_set_margin_right (w, 20);
  gtk_widget_set_margin_top (w, 12);
  gtk_widget_set_margin_bottom (w, 12);
  gtk_widget_set_halign (w, GTK_ALIGN_START);
  gtk_widget_set_valign (w, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (w, TRUE);
  gtk_box_pack_start (GTK_BOX (box), w, TRUE, TRUE, 0);
  gtk_widget_set_margin_left (status, 20);
  gtk_widget_set_margin_right (status, 20);
  gtk_widget_set_halign (status, GTK_ALIGN_END);
  gtk_widget_set_valign (status, GTK_ALIGN_CENTER);
  gtk_box_pack_end (GTK_BOX (box), status, FALSE, FALSE, 0);

  gtk_widget_show_all (box);
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

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "screen_lock_done"));
  dialog = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "screen_lock_dialog"));
  g_signal_connect_swapped (w, "clicked",
                            G_CALLBACK (gtk_widget_hide), dialog);
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
  g_settings_bind (self->priv->lock_settings, "show-notifications",
                   w, "active",
                   G_SETTINGS_BIND_DEFAULT);
}

static void
stealth_mode_changed (GSettings   *settings,
                      const gchar *key,
                      gpointer     data)
{
  CcPrivacyPanel *self = data;
  gboolean stealth_mode;
  GtkWidget *w;

  stealth_mode = g_settings_get_boolean (settings, "hide-identity");

  if (stealth_mode)
    {
      g_settings_set_boolean (self->priv->lock_settings, "show-full-name-in-top-bar", FALSE);
      g_settings_set_boolean (self->priv->privacy_settings, "show-full-name-in-top-bar", FALSE);
    }

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "full_name_top_bar"));
  gtk_widget_set_sensitive (w, !stealth_mode);
  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "full_name_lock_screen"));
  gtk_widget_set_sensitive (w, !stealth_mode);
  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "full_name_top_bar_label"));
  gtk_widget_set_sensitive (w, !stealth_mode);
  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "full_name_lock_screen_label"));
  gtk_widget_set_sensitive (w, !stealth_mode);
}

static void
add_name_visibility (CcPrivacyPanel *self)
{
  GtkWidget *w;
  GtkWidget *dialog;

  w = get_visible_label (self->priv->privacy_settings, "hide-identity");
  add_row (self, _("Name & Visibility"), "name_dialog", w);

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "name_done"));
  dialog = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "name_dialog"));
  g_signal_connect_swapped (w, "clicked",
                            G_CALLBACK (gtk_widget_hide), dialog);
  g_signal_connect (dialog, "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete), NULL);

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "stealth_mode"));
  g_settings_bind (self->priv->privacy_settings, "hide-identity",
                   w, "active",
                   G_SETTINGS_BIND_DEFAULT);

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "full_name_top_bar"));
  g_settings_bind (self->priv->privacy_settings, "show-full-name-in-top-bar",
                   w, "active",
                   G_SETTINGS_BIND_DEFAULT);

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "full_name_lock_screen"));
  g_settings_bind (self->priv->lock_settings, "show-full-name-in-top-bar",
                   w, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_signal_connect (self->priv->privacy_settings, "changed::hide-identity",
                    G_CALLBACK (stealth_mode_changed), self);
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

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "recent_done"));
  dialog = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "recent_dialog"));
  g_signal_connect_swapped (w, "clicked",
                            G_CALLBACK (gtk_widget_hide), dialog);
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

static void
empty_trash (CcPrivacyPanel *self)
{
  GDBusConnection *bus;
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

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "trash_done"));
  dialog = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "trash_dialog"));
  g_signal_connect_swapped (w, "clicked",
                            G_CALLBACK (gtk_widget_hide), dialog);
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
cc_privacy_panel_finalize (GObject *object)
{
  CcPrivacyPanelPrivate *priv = CC_PRIVACY_PANEL (object)->priv;

  g_clear_object (&priv->builder);
  g_clear_object (&priv->lockdown_settings);
  g_clear_object (&priv->lock_settings);
  g_clear_object (&priv->privacy_settings);

  G_OBJECT_CLASS (cc_privacy_panel_parent_class)->finalize (object);
}

static void
update_separator_func (GtkWidget **separator,
                       GtkWidget  *child,
                       GtkWidget  *before,
                       gpointer    user_data)
{
  if (before == NULL)
    return;

  if (*separator == NULL)
    {
      *separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (*separator);
      g_object_ref_sink (*separator);
    }
}

static void
activate_child (CcPrivacyPanel *self,
                GtkWidget      *child)
{
  GObject *w;
  const gchar *dialog_id;
  GtkWidget *toplevel;

  dialog_id = g_object_get_data (G_OBJECT (child), "dialog-id");
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

  frame = WID ("frame");
  widget = GTK_WIDGET (egg_list_box_new ());
  egg_list_box_set_selection_mode (EGG_LIST_BOX (widget), GTK_SELECTION_NONE);
  gtk_container_add (GTK_CONTAINER (frame), widget);
  self->priv->list_box = widget;
  gtk_widget_show (widget);

  g_signal_connect_swapped (widget, "child-activated",
                            G_CALLBACK (activate_child), self);

  egg_list_box_set_separator_funcs (EGG_LIST_BOX (widget),
                                    update_separator_func,
                                    NULL, NULL);

  self->priv->lockdown_settings = g_settings_new ("org.gnome.desktop.lockdown");
  self->priv->lock_settings = g_settings_new ("org.gnome.desktop.screensaver");
  self->priv->privacy_settings = g_settings_new ("org.gnome.desktop.privacy");

  add_screen_lock (self);
  add_name_visibility (self);
  add_usage_history (self);
  add_trash_temp (self);

  g_signal_connect (self->priv->lockdown_settings, "changed",
                    G_CALLBACK (on_lockdown_settings_changed), self);
  update_lock_screen_sensitivity (self);

  widget = WID ("privacy_vbox");
  gtk_widget_reparent (widget, (GtkWidget *) self);
}

static void
cc_privacy_panel_class_init (CcPrivacyPanelClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  oclass->finalize = cc_privacy_panel_finalize;

  g_type_class_add_private (klass, sizeof (CcPrivacyPanelPrivate));
}
