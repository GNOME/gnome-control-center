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

#include "cc-cursor-blinking-dialog.h"

#define INTERFACE_SETTINGS           "org.gnome.desktop.interface"
#define KEY_CURSOR_BLINKING          "cursor-blink"
#define KEY_CURSOR_BLINKING_TIME     "cursor-blink-time"

struct _CcCursorBlinkingDialog
{
  GtkDialog parent;

  GtkScale *blink_time_scale;
  GtkSwitch *enable_switch;

  GSettings *interface_settings;
};

G_DEFINE_TYPE (CcCursorBlinkingDialog, cc_cursor_blinking_dialog, GTK_TYPE_DIALOG);

static void
cc_cursor_blinking_dialog_dispose (GObject *object)
{
  CcCursorBlinkingDialog *self = CC_CURSOR_BLINKING_DIALOG (object);

  g_clear_object (&self->interface_settings);

  G_OBJECT_CLASS (cc_cursor_blinking_dialog_parent_class)->dispose (object);
}

static void
cc_cursor_blinking_dialog_class_init (CcCursorBlinkingDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_cursor_blinking_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/universal-access/cc-cursor-blinking-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcCursorBlinkingDialog, blink_time_scale);
  gtk_widget_class_bind_template_child (widget_class, CcCursorBlinkingDialog, enable_switch);
}

static void
cc_cursor_blinking_dialog_init (CcCursorBlinkingDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->interface_settings = g_settings_new (INTERFACE_SETTINGS);

  g_settings_bind (self->interface_settings, KEY_CURSOR_BLINKING,
                   self->enable_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->interface_settings, KEY_CURSOR_BLINKING_TIME,
                   gtk_range_get_adjustment (GTK_RANGE (self->blink_time_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);
}

CcCursorBlinkingDialog *
cc_cursor_blinking_dialog_new (void)
{
  return g_object_new (cc_cursor_blinking_dialog_get_type (),
                       "use-header-bar", TRUE,
                       NULL);
}
