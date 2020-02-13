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

#include "cc-search-panel-row.h"

struct _CcSearchPanelRow
{
  GtkListBoxRow  parent_instance;

  GAppInfo      *app_info;

  GtkEventBox   *drag_handle;
  GtkImage      *icon;
  GtkLabel      *app_name;
  GtkSwitch     *switcher;

  GtkListBox    *drag_widget;
};

G_DEFINE_TYPE (CcSearchPanelRow, cc_search_panel_row, GTK_TYPE_LIST_BOX_ROW)

enum
{
  SIGNAL_MOVE_ROW,
  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0, };

static void
move_up_button_clicked (GtkButton        *button,
                        CcSearchPanelRow *self)
{
  GtkListBox *list_box = GTK_LIST_BOX (gtk_widget_get_parent (GTK_WIDGET (self)));
  gint previous_idx = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (self)) - 1;
  GtkListBoxRow *previous_row = gtk_list_box_get_row_at_index (list_box, previous_idx);

  if (previous_row == NULL)
    return;

  g_signal_emit (self,
                 signals[SIGNAL_MOVE_ROW],
                 0,
                 previous_row);
}

static void
move_down_button_clicked (GtkButton    *button,
                          CcSearchPanelRow *self)
{
  GtkListBox *list_box = GTK_LIST_BOX (gtk_widget_get_parent (GTK_WIDGET (self)));
  gint next_idx = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (self)) + 1;
  GtkListBoxRow *next_row = gtk_list_box_get_row_at_index (list_box, next_idx);

  if (next_row == NULL)
    return;

  g_signal_emit (next_row,
                 signals[SIGNAL_MOVE_ROW],
                 0,
                 self);
}

static void
drag_begin_cb (CcSearchPanelRow *self,
               GdkDragContext   *drag_context)
{
  CcSearchPanelRow *drag_row;
  GtkAllocation alloc;
  gint x = 0, y = 0;

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  gdk_window_get_device_position (gtk_widget_get_window (GTK_WIDGET (self)),
                                  gdk_drag_context_get_device (drag_context),
                                  &x, &y, NULL);

  self->drag_widget = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_widget_show (GTK_WIDGET (self->drag_widget));
  gtk_widget_set_size_request (GTK_WIDGET (self->drag_widget), alloc.width, alloc.height);

  drag_row = cc_search_panel_row_new (self->app_info);
  gtk_switch_set_active (drag_row->switcher, gtk_switch_get_active (self->switcher));
  gtk_widget_show (GTK_WIDGET (drag_row));
  gtk_container_add (GTK_CONTAINER (self->drag_widget), GTK_WIDGET (drag_row));
  gtk_list_box_drag_highlight_row (self->drag_widget, GTK_LIST_BOX_ROW (drag_row));

  gtk_drag_set_icon_widget (drag_context, GTK_WIDGET (self->drag_widget), x - alloc.x, y - alloc.y);
}

static void
drag_end_cb (CcSearchPanelRow *self)
{
  g_clear_pointer ((GtkWidget **) &self->drag_widget, gtk_widget_destroy);
}

static void
drag_data_get_cb (CcSearchPanelRow *self,
                  GdkDragContext   *context,
                  GtkSelectionData *selection_data,
                  guint             info,
                  guint             time_)
{
  gtk_selection_data_set (selection_data,
                          gdk_atom_intern_static_string ("GTK_LIST_BOX_ROW"),
                          32,
                          (const guchar *)&self,
                          sizeof (gpointer));
}

static void
drag_data_received_cb (CcSearchPanelRow *self,
                       GdkDragContext   *context,
                       gint              x,
                       gint              y,
                       GtkSelectionData *selection_data,
                       guint             info,
                       guint             time_)
{
  CcSearchPanelRow *source;

  source = *((CcSearchPanelRow **) gtk_selection_data_get_data (selection_data));
  if (source == self)
    return;

  g_signal_emit (source,
                 signals[SIGNAL_MOVE_ROW],
                 0,
                 self);
}

static GtkTargetEntry entries[] =
{
  { "GTK_LIST_BOX_ROW", GTK_TARGET_SAME_APP, 0 }
};

static void
cc_search_panel_row_class_init (CcSearchPanelRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/search/cc-search-panel-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcSearchPanelRow, drag_handle);
  gtk_widget_class_bind_template_child (widget_class, CcSearchPanelRow, icon);
  gtk_widget_class_bind_template_child (widget_class, CcSearchPanelRow, app_name);
  gtk_widget_class_bind_template_child (widget_class, CcSearchPanelRow, switcher);

  gtk_widget_class_bind_template_callback (widget_class, drag_begin_cb);
  gtk_widget_class_bind_template_callback (widget_class, drag_end_cb);
  gtk_widget_class_bind_template_callback (widget_class, drag_data_get_cb);
  gtk_widget_class_bind_template_callback (widget_class, drag_data_received_cb);
  gtk_widget_class_bind_template_callback (widget_class, move_up_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, move_down_button_clicked);

  signals[SIGNAL_MOVE_ROW] =
    g_signal_new ("move-row",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, CC_TYPE_SEARCH_PANEL_ROW);
}

static void
cc_search_panel_row_init (CcSearchPanelRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_drag_source_set (GTK_WIDGET (self->drag_handle), GDK_BUTTON1_MASK, entries, 1, GDK_ACTION_MOVE);
  gtk_drag_dest_set (GTK_WIDGET (self), GTK_DEST_DEFAULT_ALL, entries, 1, GDK_ACTION_MOVE);
}

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

  gtk_drag_source_set (GTK_WIDGET (self->drag_handle), GDK_BUTTON1_MASK, entries, 1, GDK_ACTION_MOVE);

  return self;
}

GAppInfo *
cc_search_panel_row_get_app_info (CcSearchPanelRow *self)
{
  return self->app_info;
}

GtkWidget *
cc_search_panel_row_get_switch (CcSearchPanelRow *self)
{
  return GTK_WIDGET (self->switcher);
}
