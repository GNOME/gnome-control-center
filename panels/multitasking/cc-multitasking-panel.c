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
#include "cc-illustrated-row.h"

struct _CcMultitaskingPanel
{
  CcPanel          parent_instance;

  GSettings       *interface_settings;
  GSettings       *mutter_settings;
  GSettings       *shell_settings;
  GSettings       *wm_settings;

  CcIllustratedRow *active_screen_edges_row;
  GtkSwitch       *active_screen_edges_switch;
  GtkCheckButton  *all_workspaces_radio;
  GtkCheckButton  *current_workspace_radio;
  GtkCheckButton  *dynamic_workspaces_radio;
  GtkCheckButton  *fixed_workspaces_radio;
  CcIllustratedRow *hot_corner_row;
  GtkSwitch       *hot_corner_switch;
  AdwSwitchRow    *maximize_row;
  AdwSwitchRow    *minimize_row;
  AdwSpinRow      *number_of_workspaces_spin_row;
  AdwPreferencesGroup *workspaces_display_group;
  GtkCheckButton  *workspaces_primary_display_radio;
  GtkCheckButton  *workspaces_span_displays_radio;
};

CC_PANEL_REGISTER (CcMultitaskingPanel, cc_multitasking_panel)

static void
set_window_controls (CcMultitaskingPanel *self)
{
  g_autofree gchar *layout = NULL;
  g_autofree gchar *topbar_layout = NULL;
  gboolean maximize, minimize;

  maximize = adw_switch_row_get_active (self->maximize_row);;
  minimize = adw_switch_row_get_active (self->minimize_row);;

  if (minimize && maximize)
    layout = g_strdup ("minimize,maximize,");
  else if (minimize)
    layout = g_strdup ("minimize,");
  else if (maximize)
    layout = g_strdup ("maximize,");
  else
    layout = g_strdup ("");

  topbar_layout = g_strdup_printf ("appmnenu:%sclose", layout);
  g_settings_set_string (self->wm_settings,
                         "button-layout",
                         topbar_layout);
}

static void
get_window_controls (CcMultitaskingPanel *self)
{
  g_autofree gchar *layout = NULL;

  layout = g_settings_get_string (self->wm_settings, "button-layout");

  g_signal_handlers_block_by_func (self->maximize_row, set_window_controls, self);
  g_signal_handlers_block_by_func (self->minimize_row, set_window_controls, self);

  adw_switch_row_set_active (self->maximize_row, g_strrstr (layout, "maximize") != NULL);
  adw_switch_row_set_active (self->minimize_row, g_strrstr (layout, "minimize") != NULL);

  g_signal_handlers_unblock_by_func (self->maximize_row, set_window_controls, self);
  g_signal_handlers_unblock_by_func (self->minimize_row, set_window_controls, self);
}

static void
fixed_workspaces_changed_cb (CcMultitaskingPanel *self)
{
  gboolean multi_workspaces, fixed_workspaces;

  multi_workspaces = (adw_spin_row_get_value (self->number_of_workspaces_spin_row) > 1);
  fixed_workspaces = gtk_check_button_get_active (self->fixed_workspaces_radio);

  gtk_widget_set_sensitive (GTK_WIDGET (self->workspaces_display_group),
                            multi_workspaces || !fixed_workspaces);
}

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

  object_class->finalize = cc_multitasking_panel_finalize;

  g_type_ensure (CC_TYPE_ILLUSTRATED_ROW);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/multitasking/cc-multitasking-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, active_screen_edges_row);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, active_screen_edges_switch);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, all_workspaces_radio);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, current_workspace_radio);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, dynamic_workspaces_radio);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, fixed_workspaces_radio);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, hot_corner_row);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, hot_corner_switch);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, number_of_workspaces_spin_row);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, maximize_row);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, minimize_row);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, workspaces_display_group);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, workspaces_primary_display_radio);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, workspaces_span_displays_radio);

  gtk_widget_class_bind_template_callback (widget_class, fixed_workspaces_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, set_window_controls);
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
    gtk_check_button_set_active (self->workspaces_primary_display_radio, TRUE);
  else
    gtk_check_button_set_active (self->workspaces_span_displays_radio, TRUE);

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
    gtk_check_button_set_active (self->dynamic_workspaces_radio, TRUE);
  else
    gtk_check_button_set_active (self->fixed_workspaces_radio, TRUE);

  g_settings_bind (self->mutter_settings,
                   "dynamic-workspaces",
                   self->dynamic_workspaces_radio,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  self->wm_settings = g_settings_new ("org.gnome.desktop.wm.preferences");
  g_settings_bind (self->wm_settings,
                   "num-workspaces",
                   self->number_of_workspaces_spin_row,
                   "value",
                   G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);
  g_signal_connect_swapped (self->wm_settings,
                            "changed::button-layout",
                            G_CALLBACK (get_window_controls),
                            self);
  get_window_controls (self);

  self->shell_settings = g_settings_new ("org.gnome.shell.app-switcher");

  if (g_settings_get_boolean (self->shell_settings, "current-workspace-only"))
    gtk_check_button_set_active (self->current_workspace_radio, TRUE);
  else
    gtk_check_button_set_active (self->all_workspaces_radio, TRUE);

  g_settings_bind (self->shell_settings,
                   "current-workspace-only",
                   self->current_workspace_radio,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  if (gtk_widget_get_default_direction () == GTK_TEXT_DIR_RTL)
    {
      cc_illustrated_row_set_resource (self->hot_corner_row,
                                       "/org/gnome/control-center/multitasking/assets/hot-corner-rtl.svg");
      cc_illustrated_row_set_resource (self->active_screen_edges_row,
                                       "/org/gnome/control-center/multitasking/assets/active-screen-edges-rtl.svg");
    }
}
