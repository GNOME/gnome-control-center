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

#include <egg-list-box/egg-list-box.h>
#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

CC_PANEL_REGISTER (CcPrivacyPanel, cc_privacy_panel)

#define WID(s) GTK_WIDGET (gtk_builder_get_object (self->priv->builder, s))

struct _CcPrivacyPanelPrivate
{
  GtkBuilder *builder;
  GtkWidget  *list_box;

  GSettings  *lockdown_settings;
  GSettings  *lock_settings;
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

static void
add_row (CcPrivacyPanel *self,
         const gchar    *label,
         const gchar    *dialog_id,
         GtkWidget      *status)
{
  GtkWidget *box, *w;

  gtk_widget_set_valign (self->priv->list_box, GTK_ALIGN_FILL);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  g_object_set_data (G_OBJECT (box), "dialog-id", (gpointer)dialog_id);
  gtk_widget_set_hexpand (box, TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (box), 6);
  gtk_container_add (GTK_CONTAINER (self->priv->list_box), box);

  w = gtk_label_new (label);
  gtk_container_add (GTK_CONTAINER (box), w);
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

  self->priv->lock_settings = g_settings_new ("org.gnome.desktop.screensaver");
  w = get_on_off_label (self->priv->lock_settings, "lock-enabled");
  add_row (self, _("Screen Lock"), "screen_lock_dialog", w);

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "screen_lock_done"));
  dialog = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "screen_lock_dialog"));
  g_signal_connect_swapped (w, "clicked",
                            G_CALLBACK (gtk_widget_hide), dialog);

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "automatic_screen_lock"));
  g_settings_bind (self->priv->lock_settings, "lock-enabled",
                   w, "active",
                   G_SETTINGS_BIND_DEFAULT);

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "lock_after_label"));
  g_settings_bind (self->priv->lock_settings, "lock-enabled",
                   w, "sensitive",
                   G_SETTINGS_BIND_GET);

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "lock_after_combo"));
  g_settings_bind (self->priv->lock_settings, "lock-enabled",
                   w, "sensitive",
                   G_SETTINGS_BIND_GET);

  set_lock_value_for_combo (GTK_COMBO_BOX (w), self);
  g_signal_connect (w, "changed",
                    G_CALLBACK (lock_combo_changed_cb), self);

  w = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "show_notifications"));
  g_settings_bind (self->priv->lock_settings, "show-notifications",
                   w, "active",
                   G_SETTINGS_BIND_DEFAULT);
}

static void
cc_privacy_panel_finalize (GObject *object)
{
  CcPrivacyPanelPrivate *priv = CC_PRIVACY_PANEL (object)->priv;

  g_clear_object (&priv->builder);
  g_clear_object (&priv->lockdown_settings);
  g_clear_object (&priv->lock_settings);

  G_OBJECT_CLASS (cc_privacy_panel_parent_class)->finalize (object);
}

static void
update_separator_func (GtkWidget **separator,
                       GtkWidget  *child,
                       GtkWidget  *before,
                       gpointer    user_data)
{
  if (*separator == NULL)
    {
      *separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (*separator);
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
  GtkWidget *scrolled_window;
  guint res;

  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, CC_TYPE_PRIVACY_PANEL, CcPrivacyPanelPrivate);

  self->priv->builder = gtk_builder_new ();

  error = NULL;
  res = gtk_builder_add_from_file (self->priv->builder,
                                   GNOMECC_UI_DIR "/privacy.ui",
                                   &error);

  if (res == 0)
    {
      g_warning ("Could not load interface file: %s",
                 (error != NULL) ? error->message : "unknown error");
      g_clear_error (&error);
      return;
    }

  scrolled_window = WID ("scrolled_window");
  widget = GTK_WIDGET (egg_list_box_new ());
  egg_list_box_add_to_scrolled (EGG_LIST_BOX (widget), GTK_SCROLLED_WINDOW (scrolled_window));
  self->priv->list_box = widget;
  gtk_widget_show (widget);

  g_signal_connect_swapped (widget, "child-activated",
                            G_CALLBACK (activate_child), self);

  egg_list_box_set_separator_funcs (EGG_LIST_BOX (widget),
                                    update_separator_func,
                                    NULL, NULL);

  add_screen_lock (self);

  self->priv->lockdown_settings = g_settings_new ("org.gnome.desktop.lockdown");
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

void
cc_privacy_panel_register (GIOModule *module)
{
  cc_privacy_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_PRIVACY_PANEL,
                                  "privacy", 0);
}
