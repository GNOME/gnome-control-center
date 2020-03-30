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
#include "list-box-helper.h"

struct _CcMultitaskingPanel
{
  CcPanel        parent_instance;

  GSettings     *interface_settings;
  GSettings     *mutter_settings;
  GSettings     *overrides_settings;
  GSettings     *wm_settings;

  GtkSwitch     *dynamic_workspaces_switch;
  GtkSwitch     *drag_to_screen_edge_switch;
  GtkSwitch     *hot_corner_switch;
  GtkWidget     *keyboard_shortcuts_row;
  GtkSpinButton *number_of_workspaces_spin;
  GtkSwitch     *span_displays_switch;
  GtkListBox    *top_listbox;
  GtkListBox    *workspaces_listbox;
};

CC_PANEL_REGISTER (CcMultitaskingPanel, cc_multitasking_panel)

/* Callbacks */

static void
on_listbox_row_activated_cb (GtkListBox          *listbox,
                             GtkWidget           *row,
                             CcMultitaskingPanel *self)
{
  g_autoptr(GError) error = NULL;
  CcShell *shell;

  if (self->keyboard_shortcuts_row != row)
    return;

  shell = cc_panel_get_shell (CC_PANEL (self));
  cc_shell_set_active_panel_from_id (shell, "keyboard", NULL, &error);

  if (error)
    g_warning ("Error activating Keyboard Shortcuts panel: %s", error->message);
}

/* GObject overrides */

static void
cc_multitasking_panel_finalize (GObject *object)
{
  CcMultitaskingPanel *self = (CcMultitaskingPanel *)object;

  g_clear_object (&self->interface_settings);
  g_clear_object (&self->mutter_settings);
  g_clear_object (&self->overrides_settings);
  g_clear_object (&self->wm_settings);

  G_OBJECT_CLASS (cc_multitasking_panel_parent_class)->finalize (object);
}

static void
cc_multitasking_panel_class_init (CcMultitaskingPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_multitasking_panel_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/multitasking/cc-multitasking-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, dynamic_workspaces_switch);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, drag_to_screen_edge_switch);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, hot_corner_switch);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, keyboard_shortcuts_row);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, number_of_workspaces_spin);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, span_displays_switch);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, top_listbox);
  gtk_widget_class_bind_template_child (widget_class, CcMultitaskingPanel, workspaces_listbox);

  gtk_widget_class_bind_template_callback (widget_class, on_listbox_row_activated_cb);
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
  g_settings_bind (self->mutter_settings,
                   "workspaces-only-on-primary",
                   self->span_displays_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->mutter_settings,
                   "edge-tiling",
                   self->drag_to_screen_edge_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  self->overrides_settings = g_settings_new ("org.gnome.shell.overrides");
  g_settings_bind (self->overrides_settings,
                   "dynamic-workspaces",
                   self->dynamic_workspaces_switch,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  self->wm_settings = g_settings_new ("org.gnome.desktop.wm.preferences");
  g_settings_bind (self->wm_settings,
                   "num-workspaces",
                   self->number_of_workspaces_spin,
                   "value",
                   G_SETTINGS_BIND_DEFAULT);

  /* Separators */
  gtk_list_box_set_header_func (self->top_listbox, cc_list_box_update_header_func, NULL, NULL);
  gtk_list_box_set_header_func (self->workspaces_listbox, cc_list_box_update_header_func, NULL, NULL);
}
