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

#include "cc-default-apps-page.h"
#include "cc-default-apps-row.h"
#include "cc-removable-media-settings.h"

#include "shell/cc-object-storage.h"

typedef struct
{
  const char *content_type;
  /* Patterns used to filter supported mime types
     when changing preferred applications. NULL
     means no other types should be changed */
  const char *extra_type_filter;
} DefaultAppData;

struct _CcDefaultAppsPage
{
  AdwNavigationPage  parent;

  GtkWidget *web_row;
  GtkWidget *mail_row;
  GtkWidget *calendar_row;
  GtkWidget *music_row;
  GtkWidget *video_row;
  GtkWidget *photos_row;
  GtkWidget *calls_row;
  GtkWidget *sms_row;
  AdwSwitchRow *autorun_never_row;

  CcRemovableMediaSettings *removable_media_settings;

  GSettings *media_handling_settings;

#ifdef BUILD_WWAN
  MMManager *mm_manager;
#endif
};


G_DEFINE_FINAL_TYPE (CcDefaultAppsPage, cc_default_apps_page, ADW_TYPE_NAVIGATION_PAGE)

#ifdef BUILD_WWAN
static void
update_modem_apps_visibility (CcDefaultAppsPage *self)
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
cc_default_apps_page_dispose (GObject *object)
{
  CcDefaultAppsPage *self = CC_DEFAULT_APPS_PAGE (object);

  g_clear_object (&self->media_handling_settings);

#ifdef BUILD_WWAN
  g_clear_object (&self->mm_manager);
#endif

  G_OBJECT_CLASS (cc_default_apps_page_parent_class)->dispose (object);
}


static void
cc_default_apps_page_class_init (CcDefaultAppsPageClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_default_apps_page_dispose;

  g_type_ensure (CC_TYPE_REMOVABLE_MEDIA_SETTINGS);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/applications/cc-default-apps-page.ui");
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPage, web_row);
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPage, mail_row);
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPage, calendar_row);
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPage, music_row);
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPage, video_row);
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPage, photos_row);
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPage, calls_row);
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPage, sms_row);
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPage, removable_media_settings);
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPage, autorun_never_row);

  gtk_widget_class_bind_template_callback (widget_class, on_row_selected_item_changed);
}

static void
cc_default_apps_page_init (CcDefaultAppsPage *self)
{
  g_type_ensure (CC_TYPE_DEFAULT_APPS_ROW);

  gtk_widget_init_template (GTK_WIDGET (self));

  self->media_handling_settings = g_settings_new ("org.gnome.desktop.media-handling");

  g_settings_bind (self->media_handling_settings,
                   "autorun-never",
                   self->autorun_never_row,
                   "active",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);

#ifdef BUILD_WWAN
  if (cc_object_storage_has_object (CC_OBJECT_MMMANAGER))
    {
      self->mm_manager = cc_object_storage_get_object (CC_OBJECT_MMMANAGER);

      g_signal_connect_swapped (self->mm_manager, "object-added",
                                G_CALLBACK (update_modem_apps_visibility), self);
      g_signal_connect_swapped (self->mm_manager, "object-removed",
                                G_CALLBACK (update_modem_apps_visibility), self);

      update_modem_apps_visibility (self);
    }
#endif
}
