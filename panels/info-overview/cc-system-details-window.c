/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2023 Cyber Phantom <inam123451@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "cc-system-details-window.h"

struct _CcSystemDetailsWindow
{
  AdwWindow parent;

};

G_DEFINE_TYPE (CcSystemDetailsWindow, cc_system_details_window, ADW_TYPE_WINDOW)

static void
cc_system_details_window_class_init (CcSystemDetailsWindowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);
  
  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/info-overview/cc-system-details-window.ui");
}

static void
cc_system_details_window_init (CcSystemDetailsWindow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CcSystemDetailsWindow *
cc_system_details_window_new (void)
{
  return g_object_new (CC_TYPE_SYSTEM_DETAILS_WINDOW,
                       NULL);
}
