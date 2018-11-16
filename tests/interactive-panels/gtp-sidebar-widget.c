/* gtp-sidebar-widget.c
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

#include "gtp-sidebar-widget.h"

struct _GtpSidebarWidget
{
  CcPanel    parent;

  GtkWidget *sidebar_widget;
};

G_DEFINE_TYPE (GtpSidebarWidget, gtp_sidebar_widget, CC_TYPE_PANEL)

static GtkWidget*
gtp_sidebar_widget_get_sidebar_widget (CcPanel *panel)
{
  GtpSidebarWidget *self = GTP_SIDEBAR_WIDGET (panel);
  return self->sidebar_widget;
}

static void
gtp_sidebar_widget_class_init (GtpSidebarWidgetClass *klass)
{
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  panel_class->get_sidebar_widget = gtp_sidebar_widget_get_sidebar_widget;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/tests/panels/gtp-sidebar-widget.ui");

  gtk_widget_class_bind_template_child (widget_class, GtpSidebarWidget, sidebar_widget);
}

static void
gtp_sidebar_widget_init (GtpSidebarWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
