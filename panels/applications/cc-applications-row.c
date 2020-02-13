/* cc-applications-row.c
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

#include "cc-applications-row.h"
#include "cc-applications-resources.h"

struct _CcApplicationsRow
{
  GtkListBoxRow parent;

  GAppInfo     *info;
  gchar        *sortkey;

  GtkWidget    *box;
  GtkWidget    *image;
  GtkWidget    *label;
};

G_DEFINE_TYPE (CcApplicationsRow, cc_applications_row, GTK_TYPE_LIST_BOX_ROW)

static void
cc_applications_row_finalize (GObject *object)
{
  CcApplicationsRow *self = CC_APPLICATIONS_ROW (object);

  g_object_unref (self->info);
  g_free (self->sortkey);

  G_OBJECT_CLASS (cc_applications_row_parent_class)->finalize (object);
}

static void
cc_applications_row_class_init (CcApplicationsRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = cc_applications_row_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/applications/cc-applications-row.ui");

  gtk_widget_class_bind_template_child (widget_class, CcApplicationsRow, box);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsRow, image);
  gtk_widget_class_bind_template_child (widget_class, CcApplicationsRow, label);
}

static void
cc_applications_row_init (CcApplicationsRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

CcApplicationsRow *
cc_applications_row_new (GAppInfo *info)
{
  CcApplicationsRow *self;
  g_autofree gchar *key = NULL;
  GIcon *icon;

  self = g_object_new (CC_TYPE_APPLICATIONS_ROW, NULL);

  self->info = g_object_ref (info);

  key = g_utf8_casefold (g_app_info_get_display_name (info), -1);
  self->sortkey = g_utf8_collate_key (key, -1);

  icon = g_app_info_get_icon (info);
  if (icon != NULL)
    gtk_image_set_from_gicon (GTK_IMAGE (self->image), g_app_info_get_icon (info), GTK_ICON_SIZE_BUTTON);
  else
    gtk_image_set_from_icon_name (GTK_IMAGE (self->image), "application-x-executable", GTK_ICON_SIZE_BUTTON);

  gtk_label_set_label (GTK_LABEL (self->label), g_app_info_get_display_name (info));

  return self;
}

GAppInfo *
cc_applications_row_get_info (CcApplicationsRow *self)
{
  return self->info;
}

const gchar *
cc_applications_row_get_sort_key (CcApplicationsRow *self)
{
  return self->sortkey;
}
