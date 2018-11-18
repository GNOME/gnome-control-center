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

enum {
  PROP_ZERO,
  PROP_NAME
};

static int changed_signal;

struct _CcToggleRow
{
  GtkListBoxRow parent;

  GtkWidget *label;
  GtkWidget *toggle;
};

G_DEFINE_TYPE (CcToggleRow, cc_toggle_row, GTK_TYPE_LIST_BOX_ROW)

static void
cc_toggle_row_finalize (GObject *object)
{
  //CcToggleRow *row = CC_TOGGLE_ROW (object);

  G_OBJECT_CLASS (cc_toggle_row_parent_class)->finalize (object);
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
    case PROP_NAME:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (row->label)));
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
    case PROP_NAME:
      gtk_label_set_label (GTK_LABEL (row->label), g_value_get_string (value));
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

  object_class->finalize = cc_toggle_row_finalize;
  object_class->get_property = cc_toggle_row_get_property;
  object_class->set_property = cc_toggle_row_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/applications/cc-toggle-row.ui");

  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name", "name", "name",
                                                        NULL, G_PARAM_READWRITE));

  changed_signal = g_signal_new ("changed",
                          G_OBJECT_CLASS_TYPE (object_class),
                          G_SIGNAL_RUN_FIRST,
                          0,
                          NULL, NULL,
                          NULL,
                          G_TYPE_NONE, 0);

  gtk_widget_class_bind_template_child (widget_class, CcToggleRow, label);
  gtk_widget_class_bind_template_child (widget_class, CcToggleRow, toggle);
}

static void
changed_cb (GtkSwitch *toggle,
            GParamSpec *pspec,
            CcToggleRow *row)
{
  g_signal_emit (row, changed_signal, 0);
}

static void
cc_toggle_row_init (CcToggleRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (self), FALSE);
  g_signal_connect (self->toggle, "notify::active", G_CALLBACK (changed_cb), self);
}

CcToggleRow *
cc_toggle_row_new (const char *name)
{
  CcToggleRow *row;

  row = g_object_new (CC_TYPE_TOGGLE_ROW, NULL);

  gtk_label_set_label (GTK_LABEL (row->label), name);

  return row;
}

void
cc_toggle_row_set_allowed (CcToggleRow *row,
                           gboolean     allowed)
{
  gtk_switch_set_active (GTK_SWITCH (row->toggle), allowed);
}

gboolean
cc_toggle_row_get_allowed (CcToggleRow *row)
{
  return gtk_switch_get_active (GTK_SWITCH (row->toggle));
}
