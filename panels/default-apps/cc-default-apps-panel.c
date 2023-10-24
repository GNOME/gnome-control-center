/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2017 Mohammed Sadiq <sadiq@sadiqpk.org>
 * Copyright (C) 2010 Red Hat, Inc
 * Copyright (C) 2008 William Jon McCann <jmccann@redhat.com>
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
#ifdef BUILD_WWAN
#include <libmm-glib.h>
#endif

#include "cc-default-apps-panel.h"
#include "cc-default-apps-row.h"
#include "cc-default-apps-resources.h"

#include "shell/cc-object-storage.h"

typedef struct
{
  const char *content_type;
  /* Patterns used to filter supported mime types
     when changing preferred applications. NULL
     means no other types should be changed */
  const char *extra_type_filter;
} DefaultAppData;

struct _CcDefaultAppsPanel
{
  CcPanel    parent_instance;

  GtkWidget *default_apps_grid;

  GtkWidget *web_row;
  GtkWidget *mail_row;
  GtkWidget *calendar_row;
  GtkWidget *music_row;
  GtkWidget *video_row;
  GtkWidget *photos_row;
  GtkWidget *calls_row;
  GtkWidget *sms_row;

#ifdef BUILD_WWAN
  MMManager *mm_manager;
#endif
};


G_DEFINE_TYPE (CcDefaultAppsPanel, cc_default_apps_panel, CC_TYPE_PANEL)

#ifdef BUILD_WWAN
static void
update_modem_apps_visibility (CcDefaultAppsPanel *self)
{
  GList *devices;
  gboolean has_mm_objects;

  devices = g_dbus_object_manager_get_objects (G_DBUS_OBJECT_MANAGER (self->mm_manager));
  has_mm_objects = g_list_length (devices) > 0;

  gtk_widget_set_visible (self->calls_row, has_mm_objects);
  gtk_widget_set_visible (self->sms_row, has_mm_objects);

  g_list_free_full (devices, (GDestroyNotify)g_object_unref);
}
#endif

static void
on_row_selected_item_changed (CcDefaultAppsRow *row)
{
  cc_default_apps_row_update_default_app (row);
}

static void
cc_default_apps_panel_class_init (CcDefaultAppsPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/default-apps/cc-default-apps-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPanel, web_row);
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPanel, mail_row);
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPanel, calendar_row);
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPanel, music_row);
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPanel, video_row);
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPanel, photos_row);
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPanel, calls_row);
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPanel, sms_row);

  gtk_widget_class_bind_template_callback (widget_class, on_row_selected_item_changed);
}

static void
cc_default_apps_panel_init (CcDefaultAppsPanel *self)
{
  g_resources_register (cc_default_apps_get_resource ());

  g_type_ensure (CC_TYPE_DEFAULT_APPS_ROW);

  gtk_widget_init_template (GTK_WIDGET (self));

#ifdef BUILD_WWAN
  if (cc_object_storage_has_object ("CcObjectStorage::mm-manager"))
    {
      self->mm_manager = cc_object_storage_get_object ("CcObjectStorage::mm-manager");

      g_signal_connect_swapped (self->mm_manager, "object-added",
                                G_CALLBACK (update_modem_apps_visibility), self);
      g_signal_connect_swapped (self->mm_manager, "object-removed",
                                G_CALLBACK (update_modem_apps_visibility), self);

      update_modem_apps_visibility (self);
    }
#endif
}

GtkWidget *
cc_default_apps_panel_new (void)
{
  return g_object_new (CC_TYPE_DEFAULT_APPS_PANEL,
                       NULL);
}
