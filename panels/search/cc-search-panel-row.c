/*
 * Copyright © 2019 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Felipe Borges <felipeborges@gnome.org>
 */

#include <gtk/gtk.h>

#include "cc-search-panel-row.h"

G_DEFINE_TYPE (CcSearchPanelRow, cc_search_panel_row, GTK_TYPE_LIST_BOX_ROW)

CcSearchPanelRow *
cc_search_panel_row_new (GAppInfo *app_info)
{
  CcSearchPanelRow *self;
  g_autoptr(GIcon) gicon = NULL;
  gint width, height;

  self = g_object_new (CC_TYPE_SEARCH_PANEL_ROW, NULL);
  self->app_info = g_object_ref (app_info);

  gicon = g_app_info_get_icon (app_info);
  if (gicon == NULL)
    gicon = g_themed_icon_new ("application-x-executable");
  else
    g_object_ref (gicon);
  gtk_image_set_from_gicon (self->icon, gicon, GTK_ICON_SIZE_DND);
  gtk_icon_size_lookup (GTK_ICON_SIZE_DND, &width, &height);
  gtk_image_set_pixel_size (self->icon, MAX (width, height));

  gtk_label_set_text (self->app_name, g_app_info_get_name (app_info));

  return self;
}

static void
cc_search_panel_row_class_init (CcSearchPanelRowClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/search/cc-search-panel-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSearchPanelRow, icon);
  gtk_widget_class_bind_template_child (widget_class, CcSearchPanelRow, app_name);
  gtk_widget_class_bind_template_child (widget_class, CcSearchPanelRow, switcher);
}

static void
cc_search_panel_row_init (CcSearchPanelRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}
