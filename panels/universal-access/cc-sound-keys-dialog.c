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

#include "cc-sound-keys-dialog.h"

#define KEYBOARD_SETTINGS            "org.gnome.desktop.a11y.keyboard"
#define KEY_TOGGLEKEYS_ENABLED       "togglekeys-enable"

struct _CcSoundKeysDialog
{
  GtkDialog parent;

  GtkSwitch *enable_switch;

  GSettings *keyboard_settings;
};

G_DEFINE_TYPE (CcSoundKeysDialog, cc_sound_keys_dialog, GTK_TYPE_DIALOG);

static void
cc_sound_keys_dialog_dispose (GObject *object)
{
  CcSoundKeysDialog *self = CC_SOUND_KEYS_DIALOG (object);

  g_clear_object (&self->keyboard_settings);

  G_OBJECT_CLASS (cc_sound_keys_dialog_parent_class)->dispose (object);
}

static void
cc_sound_keys_dialog_class_init (CcSoundKeysDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_sound_keys_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/universal-access/cc-sound-keys-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSoundKeysDialog, enable_switch);
}

static void
cc_sound_keys_dialog_init (CcSoundKeysDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->keyboard_settings = g_settings_new (KEYBOARD_SETTINGS);

  g_settings_bind (self->keyboard_settings, KEY_TOGGLEKEYS_ENABLED,
                   self->enable_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);
}

CcSoundKeysDialog *
cc_sound_keys_dialog_new (void)
{
  return g_object_new (cc_sound_keys_dialog_get_type (),
                       "use-header-bar", TRUE,
                       NULL);
}
