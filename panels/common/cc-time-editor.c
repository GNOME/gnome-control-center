/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * Copyright 2024 GNOME Foundation, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *  - Philip Withnall <pwithnall@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <glib-object.h>
#include <gtk/gtk.h>

#include "cc-time-editor.h"
#include "cc-timelike-editor.h"

/**
 * CcTimeEditor:
 *
 * A widget for displaying and editing a clock time in hours and minutes, from
 * 00:00 to 23:59.
 *
 * It also supports a 12-hour mode, which is used automatically if requested via
 * the user’s `org.gnome.desktop.interface.clock-format` GSetting.
 */
struct _CcTimeEditor {
  GtkWidget parent_instance;

  CcTimelikeEditor *editor;
};

G_DEFINE_TYPE (CcTimeEditor, cc_time_editor, GTK_TYPE_WIDGET)

static void cc_time_editor_dispose (GObject *object);

static void editor_time_changed_cb (CcTimelikeEditor *editor,
                                    gpointer          user_data);

typedef enum {
  SIGNAL_TIME_CHANGED,
} CcTimeEditorSignal;

static guint signals[SIGNAL_TIME_CHANGED + 1];

static void
cc_time_editor_class_init (CcTimeEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_time_editor_dispose;

  /**
   * CcTimeEditor::time-changed:
   *
   * Emitted when the time in the widget is edited.
   *
   * Use this rather than #GObject::notify because that would be emitted
   * separately for hours and minutes. (It’s currently not implemented.)
   */
  signals[SIGNAL_TIME_CHANGED] =
    g_signal_new ("time-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/common/cc-time-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, CcTimeEditor, editor);

  gtk_widget_class_bind_template_callback (widget_class, editor_time_changed_cb);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
}

static void
cc_time_editor_init (CcTimeEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  g_signal_connect_object (self->editor, "time-changed",
                           G_CALLBACK (editor_time_changed_cb), self, G_CONNECT_DEFAULT);
}

static void
cc_time_editor_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), CC_TYPE_TIME_EDITOR);

  G_OBJECT_CLASS (cc_time_editor_parent_class)->dispose (object);
}

static void
editor_time_changed_cb (CcTimelikeEditor *editor,
                        gpointer          user_data)
{
  CcTimeEditor *self = CC_TIME_EDITOR (user_data);

  g_signal_emit (self, signals[SIGNAL_TIME_CHANGED], 0);
}

/**
 * cc_time_editor_new:
 *
 * Create a new #CcTimeEditor.
 *
 * Returns: (transfer full): a new #CcTimeEditor
 */
CcTimeEditor *
cc_time_editor_new (void)
{
  return g_object_new (CC_TYPE_TIME_EDITOR, NULL);
}

/**
 * cc_time_editor_set_time:
 * @self: a #CcTimeEditor
 * @hour: the new hour ([0, 23])
 * @minute: the new minute ([0, 59])
 *
 * Set the time in the editor.
 *
 * This is always in 24-hour time, regardless of whether the editor is
 * displaying in 12-hour mode.
 */
void
cc_time_editor_set_time (CcTimeEditor *self,
                         guint         hour,
                         guint         minute)
{
  g_return_if_fail (CC_IS_TIME_EDITOR (self));

  cc_timelike_editor_set_time (self->editor, hour, minute);
}

/**
 * cc_time_editor_get_hour:
 * @self: a #CcTimeEditor
 *
 * Get the hours from the editor.
 *
 * These are always in 24-hour time, regardless of whether the editor is
 * displaying in 12-hour mode.
 *
 * Returns: current hours value from the editor ([0, 23])
 */
guint
cc_time_editor_get_hour (CcTimeEditor *self)
{
  g_return_val_if_fail (CC_IS_TIME_EDITOR (self), 0);

  return cc_timelike_editor_get_hour (self->editor);
}

/**
 * cc_time_editor_get_minute:
 * @self: a #CcTimeEditor
 *
 * Get the minutes from the editor.
 *
 * Returns: current minutes value from the editor ([0, 59])
 */
guint
cc_time_editor_get_minute (CcTimeEditor *self)
{
  g_return_val_if_fail (CC_IS_TIME_EDITOR (self), 0);

  return cc_timelike_editor_get_minute (self->editor);
}

