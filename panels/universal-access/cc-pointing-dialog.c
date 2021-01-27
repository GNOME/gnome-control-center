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

#include "cc-pointing-dialog.h"

#define MOUSE_SETTINGS               "org.gnome.desktop.a11y.mouse"
#define KEY_SECONDARY_CLICK_ENABLED  "secondary-click-enabled"
#define KEY_SECONDARY_CLICK_TIME     "secondary-click-time"
#define KEY_DWELL_CLICK_ENABLED      "dwell-click-enabled"
#define KEY_DWELL_TIME               "dwell-time"
#define KEY_DWELL_THRESHOLD          "dwell-threshold"

#define KEY_DOUBLE_CLICK_DELAY       "double-click"

struct _CcPointingDialog
{
  GtkDialog parent;

  GtkBox    *dwell_delay_box;
  GtkScale  *dwell_delay_scale;
  GtkBox    *dwell_threshold_box;
  GtkScale  *dwell_threshold_scale;
  GtkSwitch *hover_click_switch;
  GtkBox    *secondary_click_delay_box;
  GtkScale  *secondary_click_delay_scale;
  GtkSwitch *secondary_click_switch;

  GSettings *mouse_settings;
};

G_DEFINE_TYPE (CcPointingDialog, cc_pointing_dialog, GTK_TYPE_DIALOG);

static void
cc_pointing_dialog_dispose (GObject *object)
{
  CcPointingDialog *self = CC_POINTING_DIALOG (object);

  g_clear_object (&self->mouse_settings);

  G_OBJECT_CLASS (cc_pointing_dialog_parent_class)->dispose (object);
}

static void
cc_pointing_dialog_class_init (CcPointingDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_pointing_dialog_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/universal-access/cc-pointing-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcPointingDialog, dwell_delay_box);
  gtk_widget_class_bind_template_child (widget_class, CcPointingDialog, dwell_delay_scale);
  gtk_widget_class_bind_template_child (widget_class, CcPointingDialog, dwell_threshold_box);
  gtk_widget_class_bind_template_child (widget_class, CcPointingDialog, dwell_threshold_scale);
  gtk_widget_class_bind_template_child (widget_class, CcPointingDialog, hover_click_switch);
  gtk_widget_class_bind_template_child (widget_class, CcPointingDialog, secondary_click_delay_box);
  gtk_widget_class_bind_template_child (widget_class, CcPointingDialog, secondary_click_delay_scale);
  gtk_widget_class_bind_template_child (widget_class, CcPointingDialog, secondary_click_switch);
}

static void
cc_pointing_dialog_init (CcPointingDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  self->mouse_settings = g_settings_new (MOUSE_SETTINGS);

  /* simulated secondary click */
  g_settings_bind (self->mouse_settings, KEY_SECONDARY_CLICK_ENABLED,
                   self->secondary_click_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->mouse_settings, KEY_SECONDARY_CLICK_TIME,
                   gtk_range_get_adjustment (GTK_RANGE (self->secondary_click_delay_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  g_object_bind_property (self->secondary_click_switch, "active",
                          self->secondary_click_delay_box, "sensitive",
                          G_BINDING_SYNC_CREATE);

  /* dwell click */
  g_settings_bind (self->mouse_settings, KEY_DWELL_CLICK_ENABLED,
                   self->hover_click_switch, "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->mouse_settings, KEY_DWELL_TIME,
                   gtk_range_get_adjustment (GTK_RANGE (self->dwell_delay_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  g_object_bind_property (self->hover_click_switch, "active",
                          self->dwell_delay_box, "sensitive",
                          G_BINDING_SYNC_CREATE);

  g_settings_bind (self->mouse_settings, KEY_DWELL_THRESHOLD,
                   gtk_range_get_adjustment (GTK_RANGE (self->dwell_threshold_scale)), "value",
                   G_SETTINGS_BIND_DEFAULT);
  g_object_bind_property (self->hover_click_switch, "active",
                          self->dwell_threshold_box, "sensitive",
                          G_BINDING_SYNC_CREATE);
}

CcPointingDialog *
cc_pointing_dialog_new (void)
{
  return g_object_new (cc_pointing_dialog_get_type (),
                       "use-header-bar", TRUE,
                       NULL);
}
