/*
 * Copyright © 2018 Canonical Ltd.
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
 */

#include <config.h>
#include "cc-input-row.h"
#include "cc-input-source-ibus.h"

struct _CcInputRow
{
  GtkListBoxRow    parent_instance;

  CcInputSource   *source;

  GtkEventBox     *drag_handle;
  GtkLabel        *name_label;
  GtkButton       *remove_button;
  GtkButton       *settings_button;

  GtkListBox      *drag_widget;
};

G_DEFINE_TYPE (CcInputRow, cc_input_row, GTK_TYPE_LIST_BOX_ROW)

enum
{
  SIGNAL_SHOW_SETTINGS,
  SIGNAL_SHOW_LAYOUT,
  SIGNAL_MOVE_ROW,
  SIGNAL_REMOVE_ROW,
  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0, };

static void
drag_begin_cb (CcInputRow     *self,
               GdkDragContext *drag_context)
{
  GtkAllocation alloc;
  gint x = 0, y = 0;

  gtk_widget_get_allocation (GTK_WIDGET (self), &alloc);

  gdk_window_get_device_position (gtk_widget_get_window (GTK_WIDGET (self)),
                                  gdk_drag_context_get_device (drag_context),
                                  &x, &y, NULL);

  self->drag_widget = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_widget_show (GTK_WIDGET (self->drag_widget));
  gtk_widget_set_size_request (GTK_WIDGET (self->drag_widget), alloc.width, alloc.height);
  CcInputRow *drag_row = cc_input_row_new (self->source);
  gtk_widget_show (GTK_WIDGET (drag_row));
  gtk_container_add (GTK_CONTAINER (self->drag_widget), GTK_WIDGET (drag_row));
  gtk_list_box_drag_highlight_row (self->drag_widget, GTK_LIST_BOX_ROW (drag_row));

  gtk_drag_set_icon_widget (drag_context, GTK_WIDGET (self->drag_widget), x - alloc.x, y - alloc.y);
}

static void
drag_end_cb (CcInputRow *self)
{
  g_clear_pointer ((GtkWidget **) &self->drag_widget, gtk_widget_destroy);
}

static void
drag_data_get_cb (CcInputRow       *self,
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
drag_data_received_cb (CcInputRow       *self,
                       GdkDragContext   *context,
                       gint              x,
                       gint              y,
                       GtkSelectionData *selection_data,
                       guint             info,
                       guint             time_)
{
  CcInputRow *source;

  source = *((CcInputRow **) gtk_selection_data_get_data (selection_data));

  if (source == self)
    return;

  g_signal_emit (source,
                 signals[SIGNAL_MOVE_ROW],
                 0,
                 self);
}

static void
settings_button_clicked_cb (CcInputRow *self)
{
  g_signal_emit (self,
                 signals[SIGNAL_SHOW_SETTINGS],
                 0);
}

static void
layout_button_clicked_cb (CcInputRow *self)
{
  g_signal_emit (self,
                 signals[SIGNAL_SHOW_LAYOUT],
                 0);
}

static void
remove_button_clicked_cb (CcInputRow *self)
{
  g_signal_emit (self,
                 signals[SIGNAL_REMOVE_ROW],
                 0);
}

static void
cc_input_row_dispose (GObject *object)
{
  CcInputRow *self = CC_INPUT_ROW (object);

  g_clear_object (&self->source);

  G_OBJECT_CLASS (cc_input_row_parent_class)->dispose (object);
}

void
cc_input_row_class_init (CcInputRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_input_row_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/region/cc-input-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcInputRow, drag_handle);
  gtk_widget_class_bind_template_child (widget_class, CcInputRow, name_label);
  gtk_widget_class_bind_template_child (widget_class, CcInputRow, remove_button);
  gtk_widget_class_bind_template_child (widget_class, CcInputRow, settings_button);

  gtk_widget_class_bind_template_callback (widget_class, drag_data_get_cb);
  gtk_widget_class_bind_template_callback (widget_class, drag_begin_cb);
  gtk_widget_class_bind_template_callback (widget_class, drag_end_cb);
  gtk_widget_class_bind_template_callback (widget_class, drag_data_received_cb);
  gtk_widget_class_bind_template_callback (widget_class, layout_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, settings_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, remove_button_clicked_cb);

  signals[SIGNAL_SHOW_SETTINGS] =
    g_signal_new ("show-settings",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  signals[SIGNAL_SHOW_LAYOUT] =
    g_signal_new ("show-layout",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);

  signals[SIGNAL_MOVE_ROW] =
    g_signal_new ("move-row",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  1, CC_TYPE_INPUT_ROW);

  signals[SIGNAL_REMOVE_ROW] =
    g_signal_new ("remove-row",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);
}

static GtkTargetEntry entries[] =
{
  { "GTK_LIST_BOX_ROW", GTK_TARGET_SAME_APP, 0 }
};

void
cc_input_row_init (CcInputRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_drag_source_set (GTK_WIDGET (self->drag_handle), GDK_BUTTON1_MASK, entries, 1, GDK_ACTION_MOVE);
  gtk_drag_dest_set (GTK_WIDGET (self), GTK_DEST_DEFAULT_ALL, entries, 1, GDK_ACTION_MOVE);
}

static void
label_changed_cb (CcInputRow *self)
{
  g_autofree gchar *label = cc_input_source_get_label (self->source);
  gtk_label_set_text (self->name_label, label);
}

CcInputRow *
cc_input_row_new (CcInputSource *source)
{
  CcInputRow *self;

  self = g_object_new (CC_TYPE_INPUT_ROW, NULL);
  self->source = g_object_ref (source);

  g_signal_connect_object (source, "label-changed", G_CALLBACK (label_changed_cb), self, G_CONNECT_SWAPPED);
  label_changed_cb (self);

  gtk_widget_set_visible (GTK_WIDGET (self->settings_button), CC_IS_INPUT_SOURCE_IBUS (source));

  return self;
}

CcInputSource *
cc_input_row_get_source (CcInputRow *self)
{
  g_return_val_if_fail (CC_IS_INPUT_ROW (self), NULL);
  return self->source;
}

void
cc_input_row_set_removable (CcInputRow *self,
                            gboolean    removable)
{
  g_return_if_fail (CC_IS_INPUT_ROW (self));
  gtk_widget_set_sensitive (GTK_WIDGET (self->remove_button), removable);
}

void
cc_input_row_set_draggable (CcInputRow *self,
                            gboolean    draggable)
{
  if (draggable)
    gtk_drag_source_set (GTK_WIDGET (self->drag_handle), GDK_BUTTON1_MASK, entries, 1, GDK_ACTION_MOVE);
  else
    gtk_drag_source_unset (GTK_WIDGET (self->drag_handle));
}
