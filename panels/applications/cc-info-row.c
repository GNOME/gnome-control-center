/* cc-info-row.c
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

#include "cc-info-row.h"
#include "cc-applications-resources.h"

enum {
  PROP_ZERO,
  PROP_TITLE,
  PROP_SUBTITLE,
  PROP_INFO
};

struct _CcInfoRow
{
  GtkListBoxRow parent;

  GtkWidget *title;
  GtkWidget *subtitle;
  GtkWidget *info;
};

G_DEFINE_TYPE (CcInfoRow, cc_info_row, GTK_TYPE_LIST_BOX_ROW)

static void
cc_info_row_finalize (GObject *object)
{
  //CcInfoRow *row = CC_INFO_ROW (object);

  G_OBJECT_CLASS (cc_info_row_parent_class)->finalize (object);
}

static void
cc_info_row_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  CcInfoRow *row = CC_INFO_ROW (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (row->title)));
      break;
    case PROP_SUBTITLE:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (row->subtitle)));
      break;
    case PROP_INFO:
      g_value_set_string (value, gtk_label_get_label (GTK_LABEL (row->info)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
cc_info_row_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  CcInfoRow *row = CC_INFO_ROW (object);

  switch (prop_id)
    {
    case PROP_TITLE:
      gtk_label_set_label (GTK_LABEL (row->title), g_value_get_string (value));
      break;
    case PROP_SUBTITLE:
      gtk_label_set_label (GTK_LABEL (row->subtitle), g_value_get_string (value));
      break;
    case PROP_INFO:
      gtk_label_set_label (GTK_LABEL (row->info), g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
cc_info_row_class_init (CcInfoRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_info_row_finalize;
  object_class->get_property = cc_info_row_get_property;
  object_class->set_property = cc_info_row_set_property;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/applications/cc-info-row.ui");

  g_object_class_install_property (object_class,
                                   PROP_TITLE,
                                   g_param_spec_string ("title", "title", "title",
                                                        NULL, G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_SUBTITLE,
                                   g_param_spec_string ("subtitle", "subtitle", "subtitle",
                                                        NULL, G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_INFO,
                                   g_param_spec_string ("info", "info", "info",
                                                        NULL, G_PARAM_READWRITE));

  gtk_widget_class_bind_template_child (widget_class, CcInfoRow, title);
  gtk_widget_class_bind_template_child (widget_class, CcInfoRow, subtitle);
  gtk_widget_class_bind_template_child (widget_class, CcInfoRow, info);
}

static void
cc_info_row_init (CcInfoRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CcInfoRow *
cc_info_row_new (void)
{
  return CC_INFO_ROW (g_object_new (CC_TYPE_INFO_ROW, NULL));
}
