/* cc-action-row.c
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

#include "cc-action-row.h"
#include "cc-applications-resources.h"

enum {
  PROP_ZERO,
  PROP_NAME,
  PROP_ACTION,
  PROP_ENABLED
};

static int activated_signal;

struct _CcActionRow
{
  GtkListBoxRow parent;

  GtkWidget *label;
  GtkWidget *button;
};

G_DEFINE_TYPE (CcActionRow, cc_action_row, GTK_TYPE_LIST_BOX_ROW)

static void
cc_action_row_finalize (GObject *object)
{
  //CcActionRow *row = CC_ACTION_ROW (object);

  G_OBJECT_CLASS (cc_action_row_parent_class)->finalize (object);
}

static void
cc_action_row_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  CcActionRow *row = CC_ACTION_ROW (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (row->label)));
      break;
    case PROP_ACTION:
      g_value_set_string (value, gtk_button_get_label (GTK_BUTTON (row->button)));
      break;
    case PROP_ENABLED:
      g_value_set_boolean (value, gtk_widget_get_sensitive (row->button));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
cc_action_row_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  CcActionRow *row = CC_ACTION_ROW (object);

  switch (prop_id)
    {
    case PROP_NAME:
      gtk_label_set_label (GTK_LABEL (row->label), g_value_get_string (value));
      break;
    case PROP_ACTION:
      gtk_button_set_label (GTK_BUTTON (row->button), g_value_get_string (value));
      break;
    case PROP_ENABLED:
      gtk_widget_set_sensitive (row->button, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
cc_action_row_class_init (CcActionRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_action_row_finalize;
  object_class->get_property = cc_action_row_get_property;
  object_class->set_property = cc_action_row_set_property;

  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name", "name", "name",
                                                        NULL, G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_ACTION,
                                   g_param_spec_string ("action", "action", "action",
                                                        NULL, G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_ENABLED,
                                   g_param_spec_boolean ("enabled", "enabled", "enabled",
                                                         TRUE, G_PARAM_READWRITE));

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/applications/cc-action-row.ui");


  activated_signal = g_signal_new ("activated",
                          G_OBJECT_CLASS_TYPE (object_class),
                          G_SIGNAL_RUN_FIRST,
                          0,
                          NULL, NULL,
                          NULL,
                          G_TYPE_NONE, 0);

  gtk_widget_class_bind_template_child (widget_class, CcActionRow, label);
  gtk_widget_class_bind_template_child (widget_class, CcActionRow, button);
}

static void
clicked_cb (GtkButton *button,
            CcActionRow *row)
{
  g_signal_emit (row, activated_signal, 0);
}

static void
cc_action_row_init (CcActionRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (self), FALSE);
  g_signal_connect (self->button, "clicked", G_CALLBACK (clicked_cb), self);
}

CcActionRow *
cc_action_row_new (void)
{
  return CC_ACTION_ROW (g_object_new (CC_TYPE_ACTION_ROW, NULL));
}

void
cc_action_row_set_name (CcActionRow *row,
                        const char  *name)
{
  gtk_label_set_label (GTK_LABEL (row->label), name);
}

void
cc_action_row_set_action (CcActionRow *row,
                          const char *action,
                          gboolean sensitive)
{
  gtk_button_set_label (GTK_BUTTON (row->button), action);
  gtk_widget_set_sensitive (row->button, sensitive);
}
