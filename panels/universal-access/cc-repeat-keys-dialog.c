/*
 * Copyright 2020 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "cc-repeat-keys-dialog.h"

#define KEYBOARD_SETTINGS    "org.gnome.desktop.peripherals.keyboard"
#define KEY_REPEAT_KEYS      "repeat"
#define KEY_DELAY            "delay"
#define KEY_REPEAT_INTERVAL  "repeat-interval"

struct _CcRepeatKeysDialog
{
  GtkDialog parent;

  GtkSwitch *enable_switch;
  GtkGrid *delay_grid;
  GtkScale *delay_scale;
  GtkGrid *speed_grid;
  GtkScale *speed_scale;

  GSettings *keyboard_settings;
};

G_DEFINE_TYPE (CcRepeatKeysDialog, cc_repeat_keys_dialog, GTK_TYPE_DIALOG);

static void
on_repeat_keys_toggled (CcRepeatKeysDialog *self)
{
  gboolean on;

  on = g_settings_get_boolean (self->keyboard_settings, KEY_REPEAT_KEYS);

  gtk_widget_set_sensitive (GTK_WIDGET (self->delay_grid), on);
  gtk_widget_set_sensitive (GTK_WIDGET (self->speed_grid), on);
}

static void
cc_repeat_keys_dialog_dispose (GObject *object)
{
  CcRepeatKeysDialog *self = CC_REPEAT_KEYS_DIALOG (object);

  g_clear_object (&self->keyboard_settings);

  G_OBJECT_CLASS (cc_repeat_keys_dialog_parent_class)->dispose (object);
}

static void
cc_repeat_keys_dialog_class_init (CcRepeatKeysDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_repeat_keys_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/universal-access/cc-repeat-keys-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcRepeatKeysDialog, enable_switch);
  gtk_widget_class_bind_template_child (widget_class, CcRepeatKeysDialog, delay_grid);
  gtk_widget_class_bind_template_child (widget_class, CcRepeatKeysDialog, delay_scale);
  gtk_widget_class_bind_template_child (widget_class, CcRepeatKeysDialog, speed_grid);
  gtk_widget_class_bind_template_child (widget_class, CcRepeatKeysDialog, speed_scale);
}

static void
cc_repeat_keys_dialog_init (CcRepeatKeysDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->keyboard_settings = g_settings_new (KEYBOARD_SETTINGS);

  g_signal_connect_object (self->keyboard_settings, "changed",
                           G_CALLBACK (on_repeat_keys_toggled), self, G_CONNECT_SWAPPED);
  on_repeat_keys_toggled (self);

  g_settings_bind (self->keyboard_settings, KEY_REPEAT_KEYS,
                   self->enable_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->keyboard_settings, KEY_DELAY,
                   gtk_range_get_adjustment (GTK_RANGE (self->delay_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->keyboard_settings, KEY_REPEAT_INTERVAL,
                   gtk_range_get_adjustment (GTK_RANGE (self->speed_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);
}

CcRepeatKeysDialog *
cc_repeat_keys_dialog_new (void)
{
  return g_object_new (cc_repeat_keys_dialog_get_type (),
                       "use-header-bar", TRUE,
                       NULL);
}
