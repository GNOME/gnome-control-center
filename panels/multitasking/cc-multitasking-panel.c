/* cc-multitasking-panel.h
 *
 * Copyright 2020 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "cc-multitasking-panel.h"

#include "cc-multitasking-resources.h"
#include "cc-multitasking-row.h"
#include "list-box-helper.h"

struct _CcMultitaskingPanel
{
  CcPanel          parent_instance;

  GSettings       *interface_settings;
  GSettings       *mutter_settings;
  GSettings       *shell_settings;
  GSettings       *wm_settings;

  GtkSwitch       *active_screen_edges_switch;
  GtkToggleButton *current_workspace_radio;
  GtkToggleButton *dynamic_workspaces_radio;
  GtkToggleButton *fixed_workspaces_radio;
  GtkSwitch       *hot_corner_switch;
  GtkSpinButton   *number_of_workspaces_spin;
  GtkToggleButton *workspaces_primary_display_radio;
  GtkToggleButton *workspaces_span_displays_radio;
};

CC_PANEL_REGISTER (CcMultitaskingPanel, cc_multitasking_panel)

/* GObject overrides */

static void
cc_multitasking_panel_finalize (GObject *object)
{
  CcMultitaskingPanel *self = (CcMultitaskingPanel *)object;

  g_clear_object (&self->interface_settings);
  g_clear_object (&self->mutter_settings);
  g_clear_object (&self->shell_settings);
  g_clear_object (&self->wm_settings);

  G_OBJECT_CLASS (cc_multitasking_panel_parent_class)->finalize (object);
}

static void
cc_multitasking_panel_class_init (CcMultitaskingPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  g_type_ensure (CC_TYPE_MULTITASKING_ROW);

  object_class->finalize = cc_multitasking_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/multitasking/cc-multitasking-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, active_screen_edges_switch);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, current_workspace_radio);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, dynamic_workspaces_radio);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, fixed_workspaces_radio);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, hot_corner_switch);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, number_of_workspaces_spin);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, workspaces_primary_display_radio);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, workspaces_span_displays_radio);
}

static void
cc_multitasking_panel_init (CcMultitaskingPanel *self)
{
  g_resources_register (cc_multitasking_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->interface_settings = g_settings_new ("org.gnome.desktop.interface");
  g_settings_bind (self->interface_settings,
                   "enable-hot-corners",
                   self->hot_corner_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  self->mutter_settings = g_settings_new ("org.gnome.mutter");

  if (g_settings_get_boolean (self->mutter_settings, "workspaces-only-on-primary"))
    gtk_toggle_button_set_active (self->workspaces_primary_display_radio, TRUE);
  else
    gtk_toggle_button_set_active (self->workspaces_span_displays_radio, TRUE);

  g_settings_bind (self->mutter_settings,
                   "workspaces-only-on-primary",
                   self->workspaces_primary_display_radio,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->mutter_settings,
                   "edge-tiling",
                   self->active_screen_edges_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  if (g_settings_get_boolean (self->mutter_settings, "dynamic-workspaces"))
    gtk_toggle_button_set_active (self->dynamic_workspaces_radio, TRUE);
  else
    gtk_toggle_button_set_active (self->fixed_workspaces_radio, TRUE);

  g_settings_bind (self->mutter_settings,
                   "dynamic-workspaces",
                   self->dynamic_workspaces_radio,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  self->wm_settings = g_settings_new ("org.gnome.desktop.wm.preferences");
  g_settings_bind (self->wm_settings,
                   "num-workspaces",
                   self->number_of_workspaces_spin,
                   "value",
                   G_SETTINGS_BIND_DEFAULT);

  self->shell_settings = g_settings_new ("org.gnome.shell.app-switcher");

  if (g_settings_get_boolean (self->shell_settings, "current-workspace-only"))
    gtk_toggle_button_set_active (self->current_workspace_radio, TRUE);

  g_settings_bind (self->shell_settings,
                   "current-workspace-only",
                   self->current_workspace_radio,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
}
