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

#include "cc-duration-editor.h"
#include "cc-timelike-editor.h"

/**
 * CcDurationEditor:
 *
 * An editor for time durations. It shows the hours and minutes of the duration,
 * plus buttons to increment or decrement them, and allows the user to also
 * type a duration in using the keyboard.
 *
 * Contrast with #CcTimeEditor, which is an editor for wall clock times. It
 * looks very similar, but constrains its values to [00:00, 23:59]. Durations
 * may not necessarily be constrained to that (although currently they are, as
 * no users of the widget need anything else).
 */
struct _CcDurationEditor {
  GtkWidget parent_instance;

  CcTimelikeEditor *editor;
};

G_DEFINE_TYPE (CcDurationEditor, cc_duration_editor, GTK_TYPE_WIDGET)

static void cc_duration_editor_dispose (GObject *object);

static void
cc_duration_editor_class_init (CcDurationEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_duration_editor_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/common/cc-duration-editor.ui");

  gtk_widget_class_bind_template_child (widget_class, CcDurationEditor, editor);

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BOX_LAYOUT);
}

static void
cc_duration_editor_init (CcDurationEditor *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

static void
cc_duration_editor_dispose (GObject *object)
{
  gtk_widget_dispose_template (GTK_WIDGET (object), CC_TYPE_DURATION_EDITOR);

  G_OBJECT_CLASS (cc_duration_editor_parent_class)->dispose (object);
}

/**
 * cc_duration_editor_new:
 *
 * Create a new #CcDurationEditor, to represent a duration and allow editing it.
 *
 * Returns: (transfer full): a new #CcDurationEditor
 */
CcDurationEditor *
cc_duration_editor_new (void)
{
  return g_object_new (CC_TYPE_DURATION_EDITOR, NULL);
}
