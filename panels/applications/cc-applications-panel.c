/* cc-applications-panel.c
 *
 * Copyright 2018 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cc-applications-panel.h"
#include "cc-applications-resources.h"

struct _CcApplicationsPanel
{
  CcPanel     parent;

  GtkListBox *sidebar_listbox;
};

G_DEFINE_TYPE (CcApplicationsPanel, cc_applications_panel, CC_TYPE_PANEL)

static void
cc_applications_panel_finalize (GObject *object)
{
  CcApplicationsPanel *self = (CcApplicationsPanel *)object;

  G_OBJECT_CLASS (cc_applications_panel_parent_class)->finalize (object);
}

static GtkWidget*
cc_applications_panel_get_sidebar_widget (CcPanel *panel)
{
  CcApplicationsPanel *self = CC_APPLICATIONS_PANEL (panel);
  return GTK_WIDGET (self->sidebar_listbox);
}

static void
cc_applications_panel_class_init (CcApplicationsPanelClass *klass)
{
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_applications_panel_finalize;

  panel_class->get_sidebar_widget = cc_applications_panel_get_sidebar_widget;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/applications/cc-applications-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcApplicationsPanel, sidebar_listbox);
}

static void
cc_applications_panel_init (CcApplicationsPanel *self)
{
  g_resources_register (cc_applications_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));
}
