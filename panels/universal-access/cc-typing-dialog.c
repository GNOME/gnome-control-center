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

#include "cc-typing-dialog.h"

#define KEYBOARD_SETTINGS            "org.gnome.desktop.a11y.keyboard"
#define KEY_KEYBOARD_TOGGLE          "enable"
#define KEY_STICKYKEYS_ENABLED       "stickykeys-enable"
#define KEY_STICKYKEYS_TWO_KEY_OFF   "stickykeys-two-key-off"
#define KEY_STICKYKEYS_MODIFIER_BEEP "stickykeys-modifier-beep"
#define KEY_SLOWKEYS_ENABLED         "slowkeys-enable"
#define KEY_SLOWKEYS_DELAY           "slowkeys-delay"
#define KEY_SLOWKEYS_BEEP_PRESS      "slowkeys-beep-press"
#define KEY_SLOWKEYS_BEEP_ACCEPT     "slowkeys-beep-accept"
#define KEY_SLOWKEYS_BEEP_REJECT     "slowkeys-beep-reject"
#define KEY_BOUNCEKEYS_ENABLED       "bouncekeys-enable"
#define KEY_BOUNCEKEYS_DELAY         "bouncekeys-delay"
#define KEY_BOUNCEKEYS_BEEP_REJECT   "bouncekeys-beep-reject"

struct _CcTypingDialog
{
  GtkDialog parent;

  GtkCheckButton *bouncekeys_beep_rejected_check;
  GtkBox         *bouncekeys_delay_box;
  GtkScale       *bouncekeys_delay_scale;
  GtkSwitch      *bouncekeys_switch;
  GtkSwitch      *keyboard_toggle_switch;
  GtkCheckButton *slowkeys_beep_accepted_check;
  GtkCheckButton *slowkeys_beep_pressed_check;
  GtkCheckButton *slowkeys_beep_rejected_check;
  GtkBox         *slowkeys_delay_box;
  GtkScale       *slowkeys_delay_scale;
  GtkSwitch      *slowkeys_switch;
  GtkCheckButton *stickykeys_beep_modifier_check;
  GtkCheckButton *stickykeys_disable_two_keys_check;
  GtkSwitch      *stickykeys_switch;

  GSettings *keyboard_settings;
};

G_DEFINE_TYPE (CcTypingDialog, cc_typing_dialog, GTK_TYPE_DIALOG);

static void
cc_typing_dialog_dispose (GObject *object)
{
  CcTypingDialog *self = CC_TYPING_DIALOG (object);

  g_clear_object (&self->keyboard_settings);

  G_OBJECT_CLASS (cc_typing_dialog_parent_class)->dispose (object);
}

static void
cc_typing_dialog_class_init (CcTypingDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_typing_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/universal-access/cc-typing-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcTypingDialog, bouncekeys_beep_rejected_check);
  gtk_widget_class_bind_template_child (widget_class, CcTypingDialog, bouncekeys_delay_box);
  gtk_widget_class_bind_template_child (widget_class, CcTypingDialog, bouncekeys_delay_scale);
  gtk_widget_class_bind_template_child (widget_class, CcTypingDialog, bouncekeys_switch);
  gtk_widget_class_bind_template_child (widget_class, CcTypingDialog, keyboard_toggle_switch);
  gtk_widget_class_bind_template_child (widget_class, CcTypingDialog, slowkeys_beep_accepted_check);
  gtk_widget_class_bind_template_child (widget_class, CcTypingDialog, slowkeys_beep_pressed_check);
  gtk_widget_class_bind_template_child (widget_class, CcTypingDialog, slowkeys_beep_rejected_check);
  gtk_widget_class_bind_template_child (widget_class, CcTypingDialog, slowkeys_delay_box);
  gtk_widget_class_bind_template_child (widget_class, CcTypingDialog, slowkeys_delay_scale);
  gtk_widget_class_bind_template_child (widget_class, CcTypingDialog, slowkeys_switch);
  gtk_widget_class_bind_template_child (widget_class, CcTypingDialog, stickykeys_beep_modifier_check);
  gtk_widget_class_bind_template_child (widget_class, CcTypingDialog, stickykeys_disable_two_keys_check);
  gtk_widget_class_bind_template_child (widget_class, CcTypingDialog, stickykeys_switch);
}

static void
cc_typing_dialog_init (CcTypingDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->keyboard_settings = g_settings_new (KEYBOARD_SETTINGS);

  /* enable shortcuts */
  g_settings_bind (self->keyboard_settings, KEY_KEYBOARD_TOGGLE,
                   self->keyboard_toggle_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  /* sticky keys */
  g_settings_bind (self->keyboard_settings, KEY_STICKYKEYS_ENABLED,
                   self->stickykeys_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->keyboard_settings, KEY_STICKYKEYS_TWO_KEY_OFF,
                   self->stickykeys_disable_two_keys_check, "active",
                   G_SETTINGS_BIND_NO_SENSITIVITY);
  g_object_bind_property (self->stickykeys_switch, "active",
                          self->stickykeys_disable_two_keys_check, "sensitive",
                          G_BINDING_SYNC_CREATE);

  g_settings_bind (self->keyboard_settings, KEY_STICKYKEYS_MODIFIER_BEEP,
                   self->stickykeys_beep_modifier_check, "active",
                   G_SETTINGS_BIND_NO_SENSITIVITY);
  g_object_bind_property (self->stickykeys_switch, "active",
                          self->stickykeys_beep_modifier_check, "sensitive",
                          G_BINDING_SYNC_CREATE);

  /* slow keys */
  g_settings_bind (self->keyboard_settings, KEY_SLOWKEYS_ENABLED,
                   self->slowkeys_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->keyboard_settings, KEY_SLOWKEYS_DELAY,
                   gtk_range_get_adjustment (GTK_RANGE (self->slowkeys_delay_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  g_object_bind_property (self->slowkeys_switch, "active",
                          self->slowkeys_delay_box, "sensitive",
                          G_BINDING_SYNC_CREATE);

  g_settings_bind (self->keyboard_settings, KEY_SLOWKEYS_BEEP_PRESS,
                   self->slowkeys_beep_pressed_check, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_object_bind_property (self->slowkeys_switch, "active",
                          self->slowkeys_beep_pressed_check, "sensitive",
                          G_BINDING_SYNC_CREATE);

  g_settings_bind (self->keyboard_settings, KEY_SLOWKEYS_BEEP_ACCEPT,
                   self->slowkeys_beep_accepted_check, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_object_bind_property (self->slowkeys_switch, "active",
                          self->slowkeys_beep_accepted_check, "sensitive",
                          G_BINDING_SYNC_CREATE);

  g_settings_bind (self->keyboard_settings, KEY_SLOWKEYS_BEEP_REJECT,
                   self->slowkeys_beep_rejected_check, "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_object_bind_property (self->slowkeys_switch, "active",
                          self->slowkeys_beep_rejected_check, "sensitive",
                          G_BINDING_SYNC_CREATE);

  /* bounce keys */
  g_settings_bind (self->keyboard_settings, KEY_BOUNCEKEYS_ENABLED,
                   self->bouncekeys_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->keyboard_settings, KEY_BOUNCEKEYS_DELAY,
                   gtk_range_get_adjustment (GTK_RANGE (self->bouncekeys_delay_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  g_object_bind_property (self->bouncekeys_switch, "active",
                          self->bouncekeys_delay_box, "sensitive",
                          G_BINDING_SYNC_CREATE);

  g_settings_bind (self->keyboard_settings, KEY_BOUNCEKEYS_BEEP_REJECT,
                   self->bouncekeys_beep_rejected_check, "active",
                   G_SETTINGS_BIND_NO_SENSITIVITY);
  g_object_bind_property (self->bouncekeys_switch, "active",
                          self->bouncekeys_beep_rejected_check, "sensitive",
                          G_BINDING_SYNC_CREATE);
}

CcTypingDialog *
cc_typing_dialog_new (void)
{
  return g_object_new (cc_typing_dialog_get_type (),
                       "use-header-bar", TRUE,
                       NULL);
}
