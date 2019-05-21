/* gtp-header-widget.c
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

#include "gtp-header-widget.h"

struct _GtpHeaderWidget
{
  CcPanel    parent;

  GtkWidget *header_widget;
};

G_DEFINE_TYPE (GtpHeaderWidget, gtp_header_widget, CC_TYPE_PANEL)

static void
gtp_header_widget_constructed (GObject *object)
{
  GtpHeaderWidget *self = (GtpHeaderWidget *)object;
  CcShell *shell;

  G_OBJECT_CLASS (gtp_header_widget_parent_class)->constructed (object);

  shell = cc_panel_get_shell (CC_PANEL (self));
  cc_shell_embed_widget_in_header (shell, self->header_widget, GTK_POS_LEFT);
}

static void
gtp_header_widget_class_init (GtpHeaderWidgetClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->constructed = gtp_header_widget_constructed;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/tests/panels/gtp-header-widget.ui");

  gtk_widget_class_bind_template_child (widget_class, GtpHeaderWidget, header_widget);
}

static void
gtp_header_widget_init (GtpHeaderWidget *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
