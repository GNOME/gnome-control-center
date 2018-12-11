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
#include "cc-lock-panel.h"
#include "cc-lock-resources.h"
#include "cc-util.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

struct _CcLockPanel
{
  CcPanel     parent_instance;

  GSettings  *lock_settings;
  GSettings  *notification_settings;

  GtkListBox *lock_list_box;
  GtkSwitch   *automatic_screen_lock_switch;
  GtkSwitch   *show_notifications_switch;
  GtkComboBox *lock_after_combo;
};

CC_PANEL_REGISTER (CcLockPanel, cc_lock_panel)

static void
cc_lock_panel_finalize (GObject *object)
{
  CcLockPanel *self = CC_LOCK_PANEL (object);

  g_clear_object (&self->lock_settings);

  G_OBJECT_CLASS (cc_lock_panel_parent_class)->finalize (object);
}

static void
lock_combo_changed_cb (GtkWidget *widget,
                       CcLockPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  guint delay;
  gboolean ret;

  ret = gtk_combo_box_get_active_iter (GTK_COMBO_BOX (widget), &iter);
  if (!ret)
    return;

  model = gtk_combo_box_get_model (GTK_COMBO_BOX (widget));
  gtk_tree_model_get (model, &iter,
                      1, &delay,
                      -1);
  g_settings_set (self->lock_settings, "lock-delay", "u", delay);
}

static void
set_lock_value_for_combo (GtkComboBox *combo_box,
                          CcLockPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  guint value;
  gint value_tmp, value_prev;
  gboolean ret;
  guint i;

  model = gtk_combo_box_get_model (combo_box);
  ret = gtk_tree_model_get_iter_first (model, &iter);
  if (!ret)
    return;

  value_prev = 0;
  i = 0;

  g_settings_get (self->lock_settings, "lock-delay", "u", &value);
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

  gtk_combo_box_set_active (combo_box, i - 1);
}

static void
cc_lock_panel_init (CcLockPanel *self)
{
  g_resources_register (cc_lock_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (self->lock_list_box,
                                cc_list_box_update_header_func,
                                NULL, NULL);

  self->lock_settings = g_settings_new ("org.gnome.desktop.screensaver");
  self->notification_settings = g_settings_new ("org.gnome.desktop.notifications");

  g_settings_bind (self->lock_settings, "lock-enabled",
                   self->automatic_screen_lock_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->lock_settings, "lock-enabled",
                   self->lock_after_combo, "sensitive",
                   G_SETTINGS_BIND_GET);

  set_lock_value_for_combo (self->lock_after_combo, self);
  g_signal_connect (self->lock_after_combo, "changed",
                    G_CALLBACK (lock_combo_changed_cb), self);

  g_settings_bind (self->notification_settings, "show-in-lock-screen",
                   self->show_notifications_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
}

static void
cc_lock_panel_class_init (CcLockPanelClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  oclass->finalize = cc_lock_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/lock/cc-lock-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcLockPanel, lock_list_box);
  gtk_widget_class_bind_template_child (widget_class, CcLockPanel, automatic_screen_lock_switch);
  gtk_widget_class_bind_template_child (widget_class, CcLockPanel, show_notifications_switch);
  gtk_widget_class_bind_template_child (widget_class, CcLockPanel, lock_after_combo);
}
