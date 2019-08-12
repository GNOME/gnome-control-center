/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2017 Mohammed Sadiq <sadiq@sadiqpk.org>
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
 * Copyright © 2019 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <config.h>
#include <glib/gi18n.h>

#include "cc-default-app-row.h"
#include "cc-default-apps-panel.h"
#include "cc-default-apps-resources.h"
#include "list-box-helper.h"

typedef struct
{
  const char *content_type;
  gint label_offset;
  /* Patterns used to filter supported mime types
     when changing preferred applications. NULL
     means no other types should be changed */
  const char *extra_type_filter;
} DefaultAppData;

struct _CcDefaultAppsPanel
{
  CcPanel       parent_instance;

  GtkListBox   *app_list;
  GtkSizeGroup *app_size_group;
};

G_DEFINE_TYPE (CcDefaultAppsPanel, cc_default_apps_panel, CC_TYPE_PANEL)

static void
cc_default_apps_panel_class_init (CcDefaultAppsPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/default-apps/cc-default-apps-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPanel, app_list);
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPanel, app_size_group);
}

static void
cc_default_apps_panel_init (CcDefaultAppsPanel *self)
{
  CcDefaultAppRow *row;

  g_resources_register (cc_default_apps_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_list_box_set_header_func (self->app_list,
                                cc_list_box_update_header_func,
                                NULL, NULL);

  row = cc_default_app_row_new ("x-scheme-handler/http", "text/html;application/xhtml+xml;x-scheme-handler/https", _("_Web"), self->app_size_group);
  gtk_widget_show (GTK_WIDGET (row));
  gtk_container_add (GTK_CONTAINER (self->app_list), GTK_WIDGET (row));
  row = cc_default_app_row_new ("x-scheme-handler/mailto", "x-scheme-handler/mailto", _("_Mail"), self->app_size_group);
  gtk_widget_show (GTK_WIDGET (row));
  gtk_container_add (GTK_CONTAINER (self->app_list), GTK_WIDGET (row));
  row = cc_default_app_row_new ("text/calendar", "text/calendar", _("_Calendar"), self->app_size_group);
  gtk_widget_show (GTK_WIDGET (row));
  gtk_container_add (GTK_CONTAINER (self->app_list), GTK_WIDGET (row));
  row = cc_default_app_row_new ("audio/x-vorbis+ogg", "audio/*", _("M_usic"), self->app_size_group);
  gtk_widget_show (GTK_WIDGET (row));
  gtk_container_add (GTK_CONTAINER (self->app_list), GTK_WIDGET (row));
  row = cc_default_app_row_new ("video/x-ogm+ogg", "video/*", _("_Video"), self->app_size_group);
  gtk_widget_show (GTK_WIDGET (row));
  gtk_container_add (GTK_CONTAINER (self->app_list), GTK_WIDGET (row));
  row = cc_default_app_row_new ("image/jpeg", "image/*", _("_Photos"), self->app_size_group);
  gtk_widget_show (GTK_WIDGET (row));
  gtk_container_add (GTK_CONTAINER (self->app_list), GTK_WIDGET (row));
}

GtkWidget *
cc_default_apps_panel_new (void)
{
  return g_object_new (CC_TYPE_DEFAULT_APPS_PANEL,
                       NULL);
}
