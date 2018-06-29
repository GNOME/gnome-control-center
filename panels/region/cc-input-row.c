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

struct _CcInputRow
{
  GtkListBoxRow    parent_instance;

  gchar           *type;
  gchar           *id;
  GDesktopAppInfo *app_info;

  GtkWidget       *name_label;
  GtkWidget       *icon_image;
};

G_DEFINE_TYPE (CcInputRow, cc_input_row, GTK_TYPE_LIST_BOX_ROW)

static void
cc_input_row_dispose (GObject *object)
{
  CcInputRow *self = CC_INPUT_ROW (object);

  g_clear_pointer (&self->type, g_free);
  g_clear_pointer (&self->id, g_free);
  g_clear_object (&self->app_info);

  G_OBJECT_CLASS (cc_input_row_parent_class)->dispose (object);
}

void
cc_input_row_class_init (CcInputRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_input_row_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/region/cc-input-row.ui");
  gtk_widget_class_bind_template_child (widget_class, CcInputRow, name_label);
  gtk_widget_class_bind_template_child (widget_class, CcInputRow, icon_image);
}

void
cc_input_row_init (CcInputRow *row)
{
  gtk_widget_init_template (GTK_WIDGET (row));
}

CcInputRow *
cc_input_row_new (const gchar     *type,
                  const gchar     *id,
                  GDesktopAppInfo *app_info)
{
  CcInputRow *row;

  row = g_object_new (CC_TYPE_INPUT_ROW, NULL);
  row->type = g_strdup (type);
  row->id = g_strdup (id);
  if (app_info != NULL)
    row->app_info = g_object_ref (app_info);

  return row;
}

const gchar *
cc_input_row_get_input_type (CcInputRow *row)
{
  g_return_val_if_fail (CC_IS_INPUT_ROW (row), NULL);
  return row->type;
}

const gchar *
cc_input_row_get_id (CcInputRow *row)
{
  g_return_val_if_fail (CC_IS_INPUT_ROW (row), NULL);
  return row->id;
}

GDesktopAppInfo *
cc_input_row_get_app_info (CcInputRow *row)
{
  g_return_val_if_fail (CC_IS_INPUT_ROW (row), NULL);
  return row->app_info;
}

void
cc_input_row_set_label (CcInputRow  *row,
                        const gchar *text)
{
  g_return_if_fail (CC_IS_INPUT_ROW (row));
  gtk_label_set_text (GTK_LABEL (row->name_label), text);
}

void
cc_input_row_set_is_input_method (CcInputRow  *row,
                                  gboolean is_input_method)
{
  g_return_if_fail (CC_IS_INPUT_ROW (row));
  gtk_widget_set_visible (row->icon_image, is_input_method);
}
