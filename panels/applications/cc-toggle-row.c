/* cc-toggle-row.c
 *
 * Copyright 2018 Matthias Clasen <matthias.clasen@gmail.com>
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

#include <config.h>
#include <glib/gi18n.h>

#include "cc-toggle-row.h"
#include "cc-applications-resources.h"

struct _CcToggleRow
{
  GtkListBoxRow parent;

  GtkWidget    *title;
  GtkWidget    *toggle;
};

G_DEFINE_TYPE (CcToggleRow, cc_toggle_row, GTK_TYPE_LIST_BOX_ROW)

enum
{
  PROP_0,
  PROP_TITLE,
  PROP_ALLOWED
};

static void
changed_cb (CcToggleRow *row)
{
  g_object_notify (G_OBJECT (row), "allowed");
}

static void
cc_toggle_row_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  CcToggleRow *row = CC_TOGGLE_ROW (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (row->title)));
      break;
    case PROP_ALLOWED:
      g_value_set_boolean (value, cc_toggle_row_get_allowed (row));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
cc_toggle_row_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  CcToggleRow *row = CC_TOGGLE_ROW (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      gtk_label_set_label (GTK_LABEL (row->title), g_value_get_string (value));
      break;
    case PROP_ALLOWED:
      cc_toggle_row_set_allowed (row, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
cc_toggle_row_class_init (CcToggleRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->get_property = cc_toggle_row_get_property;
  object_class->set_property = cc_toggle_row_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/applications/cc-toggle-row.ui");

  g_object_class_install_property (object_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title", "title", "title",
                                                        NULL, G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_ALLOWED,
                                   g_param_spec_boolean ("allowed", "allowed", "allowed",
                                                         FALSE, G_PARAM_READWRITE));

  gtk_widget_class_bind_template_child (widget_class, CcToggleRow, title);
  gtk_widget_class_bind_template_child (widget_class, CcToggleRow, toggle);

  gtk_widget_class_bind_template_callback (widget_class, changed_cb);
}

static void
cc_toggle_row_init (CcToggleRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CcToggleRow *
cc_toggle_row_new (void)
{
  return CC_TOGGLE_ROW (g_object_new (CC_TYPE_TOGGLE_ROW, NULL));
}

void
cc_toggle_row_set_allowed (CcToggleRow *self,
                           gboolean     allowed)
{
  gtk_switch_set_active (GTK_SWITCH (self->toggle), allowed);
}

gboolean
cc_toggle_row_get_allowed (CcToggleRow *self)
{
  return gtk_switch_get_active (GTK_SWITCH (self->toggle));
}
