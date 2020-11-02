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

#include <gdesktop-enums.h>

#include "cc-visual-alerts-dialog.h"

#define WM_SETTINGS                  "org.gnome.desktop.wm.preferences"
#define KEY_VISUAL_BELL_ENABLED      "visual-bell"
#define KEY_VISUAL_BELL_TYPE         "visual-bell-type"

struct _CcVisualAlertsDialog
{
  GtkDialog parent;

  GtkSwitch *enable_switch;
  GtkRadioButton *screen_radio;
  GtkButton *test_button;
  GtkRadioButton *window_radio;

  GSettings *wm_settings;
};

G_DEFINE_TYPE (CcVisualAlertsDialog, cc_visual_alerts_dialog, GTK_TYPE_DIALOG);

static void
visual_bell_type_notify_cb (CcVisualAlertsDialog *self)
{
  GtkRadioButton *widget;
  GDesktopVisualBellType type;

  type = g_settings_get_enum (self->wm_settings, KEY_VISUAL_BELL_TYPE);

  if (type == G_DESKTOP_VISUAL_BELL_FRAME_FLASH)
    widget = self->window_radio;
  else
    widget = self->screen_radio;

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), TRUE);
}

static void
visual_bell_type_toggle_cb (CcVisualAlertsDialog *self)
{
  gboolean frame_flash;
  GDesktopVisualBellType type;

  frame_flash = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->window_radio));

  if (frame_flash)
    type = G_DESKTOP_VISUAL_BELL_FRAME_FLASH;
  else
    type = G_DESKTOP_VISUAL_BELL_FULLSCREEN_FLASH;
  g_settings_set_enum (self->wm_settings, KEY_VISUAL_BELL_TYPE, type);
}

static void
test_flash (CcVisualAlertsDialog *self)
{
  GtkWidget *toplevel = gtk_widget_get_toplevel (GTK_WIDGET (self));
  gdk_window_beep (gtk_widget_get_window (toplevel));
}

static void
cc_visual_alerts_dialog_dispose (GObject *object)
{
  CcVisualAlertsDialog *self = CC_VISUAL_ALERTS_DIALOG (object);

  g_clear_object (&self->wm_settings);

  G_OBJECT_CLASS (cc_visual_alerts_dialog_parent_class)->dispose (object);
}

static void
cc_visual_alerts_dialog_class_init (CcVisualAlertsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_visual_alerts_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/universal-access/cc-visual-alerts-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcVisualAlertsDialog, enable_switch);
  gtk_widget_class_bind_template_child (widget_class, CcVisualAlertsDialog, screen_radio);
  gtk_widget_class_bind_template_child (widget_class, CcVisualAlertsDialog, test_button);
  gtk_widget_class_bind_template_child (widget_class, CcVisualAlertsDialog, window_radio);
}

static void
cc_visual_alerts_dialog_init (CcVisualAlertsDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->wm_settings = g_settings_new (WM_SETTINGS);

  /* set the initial visual bell values */
  visual_bell_type_notify_cb (self);

  /* and listen */
  g_settings_bind (self->wm_settings, KEY_VISUAL_BELL_ENABLED,
                   self->enable_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_object_bind_property (self->enable_switch, "active",
                          self->window_radio, "sensitive",
                          G_BINDING_SYNC_CREATE);

  g_object_bind_property (self->enable_switch, "active",
                          self->screen_radio, "sensitive",
                          G_BINDING_SYNC_CREATE);
  g_signal_connect_object (self->wm_settings, "changed::" KEY_VISUAL_BELL_TYPE,
                           G_CALLBACK (visual_bell_type_notify_cb), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->window_radio,
                           "toggled", G_CALLBACK (visual_bell_type_toggle_cb), self, G_CONNECT_SWAPPED);

  g_signal_connect_object (self->test_button,
                           "clicked", G_CALLBACK (test_flash), self, G_CONNECT_SWAPPED);
}

CcVisualAlertsDialog *
cc_visual_alerts_dialog_new (void)
{
  return g_object_new (cc_visual_alerts_dialog_get_type (),
                       "use-header-bar", TRUE,
                       NULL);
}
