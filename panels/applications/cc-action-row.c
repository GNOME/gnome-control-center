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

struct _CcActionRow
{
  GtkListBoxRow  parent;

  GtkWidget     *title;
  GtkWidget     *subtitle;
  GtkWidget     *button;
};

G_DEFINE_TYPE (CcActionRow, cc_action_row, GTK_TYPE_LIST_BOX_ROW)

static int activated_signal;

enum
{
  PROP_0,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_ACTION,
  PROP_ENABLED,
  PROP_DESTRUCTIVE
};

static void
clicked_cb (CcActionRow *row)
{
  g_signal_emit (row, activated_signal, 0);
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
    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (row->title)));
      break;

    case PROP_SUBTITLE:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (row->subtitle)));
      break;

    case PROP_ACTION:
      g_value_set_string (value, gtk_button_get_label (GTK_BUTTON (row->button)));
      break;

    case PROP_ENABLED:
      g_value_set_boolean (value, gtk_widget_get_sensitive (row->button));
      break;

    case PROP_DESTRUCTIVE:
      g_value_set_boolean (value,
                           gtk_style_context_has_class (gtk_widget_get_style_context (row->button), "destructive-action"));
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
    case PROP_TITLE:
      gtk_label_set_label (GTK_LABEL (row->title), g_value_get_string (value));
      break;

    case PROP_SUBTITLE:
      gtk_label_set_label (GTK_LABEL (row->subtitle), g_value_get_string (value));
      gtk_widget_set_visible (row->subtitle, strlen (g_value_get_string (value)) > 0);
      break;

    case PROP_ACTION:
      gtk_button_set_label (GTK_BUTTON (row->button), g_value_get_string (value));
      break;

    case PROP_ENABLED:
      gtk_widget_set_sensitive (row->button, g_value_get_boolean (value));
      break;

    case PROP_DESTRUCTIVE:
      if (g_value_get_boolean (value))
        gtk_style_context_add_class (gtk_widget_get_style_context (row->button), "destructive-action");
      else
        gtk_style_context_remove_class (gtk_widget_get_style_context (row->button), "destructive-action");
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

  object_class->get_property = cc_action_row_get_property;
  object_class->set_property = cc_action_row_set_property;

  g_object_class_install_property (object_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title", "title", "title",
                                                        NULL, G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_SUBTITLE,
                                   g_param_spec_string ("subtitle", "subtitle", "subtitle",
                                                        NULL, G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_ACTION,
                                   g_param_spec_string ("action", "action", "action",
                                                        NULL, G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_ENABLED,
                                   g_param_spec_boolean ("enabled", "enabled", "enabled",
                                                         TRUE, G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_DESTRUCTIVE,
                                   g_param_spec_boolean ("destructive", "destructive", "destructive",
                                                         FALSE, G_PARAM_READWRITE));

  activated_signal = g_signal_new ("activated",
                                   G_OBJECT_CLASS_TYPE (object_class),
                                   G_SIGNAL_RUN_FIRST,
                                   0,
                                   NULL, NULL,
                                   NULL,
                                   G_TYPE_NONE, 0);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/applications/cc-action-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcActionRow, title);
  gtk_widget_class_bind_template_child (widget_class, CcActionRow, subtitle);
  gtk_widget_class_bind_template_child (widget_class, CcActionRow, button);

  gtk_widget_class_bind_template_callback (widget_class, clicked_cb);
}

static void
cc_action_row_init (CcActionRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CcActionRow *
cc_action_row_new (void)
{
  return CC_ACTION_ROW (g_object_new (CC_TYPE_ACTION_ROW, NULL));
}

void
cc_action_row_set_title (CcActionRow *row,
                         const gchar *name)
{
  gtk_label_set_label (GTK_LABEL (row->title), name);
}

void
cc_action_row_set_subtitle (CcActionRow *row,
                            const gchar *name)
{
  gtk_label_set_label (GTK_LABEL (row->subtitle), name);
  gtk_widget_set_visible (row->subtitle, strlen (name) > 0);
}

void
cc_action_row_set_action (CcActionRow *row,
                          const gchar *action,
                          gboolean     sensitive)
{
  gtk_button_set_label (GTK_BUTTON (row->button), action);
  gtk_widget_set_sensitive (row->button, sensitive);
}
