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

#include "cc-default-apps-panel.h"
#include "cc-default-apps-resources.h"

#include <glib/gi18n.h>

typedef struct
{
  const char *content_type;
  /* Patterns used to filter supported mime types
     when changing preferred applications. NULL
     means no other types should be changed */
  const char *extra_type_filter;
  const char *label;
} DefaultAppData;

struct _CcDefaultAppsPanel
{
  CcPanel    parent_instance;

  AdwPreferencesGroup *group;
};


G_DEFINE_TYPE (CcDefaultAppsPanel, cc_default_apps_panel, CC_TYPE_PANEL)

static void
on_combo_row_selected_item_changedc_cb (AdwComboRow        *combo_row,
                                        GParamSpec         *pspec,
                                        CcDefaultAppsPanel *self)
{
  g_autoptr(GAppInfo) info = NULL;
  g_autoptr(GError) error = NULL;
  DefaultAppData *app_data;
  int i;

  info = adw_combo_row_get_selected_item (combo_row);
  app_data = g_object_get_data (G_OBJECT (combo_row), "cc-default-app-data");

  if (g_app_info_set_as_default_for_type (info, app_data->content_type, &error) == FALSE)
    {
      g_warning ("Failed to set '%s' as the default application for '%s': %s",
                 g_app_info_get_name (info), app_data->content_type, error->message);
    }
  else
    {
      g_debug ("Set '%s' as the default handler for '%s'",
               g_app_info_get_name (info), app_data->content_type);
    }

  if (app_data->extra_type_filter)
    {
      g_auto(GStrv) entries = NULL;
      const char *const *mime_types;
      g_autoptr(GPtrArray) patterns = NULL;

      entries = g_strsplit (app_data->extra_type_filter, ";", -1);
      patterns = g_ptr_array_new_with_free_func ((GDestroyNotify) g_pattern_spec_free);
      for (i = 0; entries[i] != NULL; i++)
        {
          GPatternSpec *pattern = g_pattern_spec_new (entries[i]);
          g_ptr_array_add (patterns, pattern);
        }

      mime_types = g_app_info_get_supported_types (info);
      for (i = 0; mime_types && mime_types[i]; i++)
        {
          int j;
          gboolean matched = FALSE;
          g_autoptr(GError) local_error = NULL;

          for (j = 0; j < patterns->len; j++)
            {
              GPatternSpec *pattern = g_ptr_array_index (patterns, j);
              if (g_pattern_match_string (pattern, mime_types[i]))
                matched = TRUE;
            }
          if (!matched)
            continue;

          if (g_app_info_set_as_default_for_type (info, mime_types[i], &local_error) == FALSE)
            {
              g_warning ("Failed to set '%s' as the default application for secondary "
                         "content type '%s': %s",
                         g_app_info_get_name (info), mime_types[i], local_error->message);
            }
          else
            {
              g_debug ("Set '%s' as the default handler for '%s'",
              g_app_info_get_name (info), mime_types[i]);
            }
        }
    }
}

static GListModel*
get_model_for_content_type (const char *content_type)
{
  g_autolist(GAppInfo) recommended_apps = NULL;
  g_autoptr(GListStore) store = NULL;
  g_autoptr(GAppInfo) default_app = NULL;
  GList *l;

  store = g_list_store_new (G_TYPE_APP_INFO);

  default_app = g_app_info_get_default_for_type (content_type, FALSE);
  if (default_app)
    g_list_store_append (store, default_app);

  recommended_apps = g_app_info_get_recommended_for_type (content_type);
  for (l = recommended_apps; l; l = l->next)
    {
      if (default_app && g_app_info_equal (default_app, l->data))
        continue;

      g_list_store_append (store, l->data);
    }

  return G_LIST_MODEL (g_steal_pointer (&store));
}

static void
on_signal_item_factory_setup_cb (GtkSignalListItemFactory *factory,
                                 GtkListItem              *item,
                                 gpointer                  user_data)
{
  GtkWidget *box, *icon, *label;

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

  icon = gtk_image_new ();
  g_object_set_data (G_OBJECT (box), "icon", icon);
  gtk_box_append (GTK_BOX (box), icon);

  label = gtk_label_new ("");
  g_object_set_data (G_OBJECT (box), "label", label);
  gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_box_append (GTK_BOX (box), label);

  gtk_list_item_set_child (item, box);
}

static void
on_signal_item_factory_bind_cb (GtkSignalListItemFactory *factory,
                                GtkListItem              *item,
                                gpointer                  user_data)
{
  GtkWidget *box, *icon, *label;
  GAppInfo *app_info;

  box = gtk_list_item_get_child (item);
  icon = g_object_get_data (G_OBJECT (box), "icon");
  label = g_object_get_data (G_OBJECT (box), "label");

  app_info = G_APP_INFO (gtk_list_item_get_item (item));
  gtk_label_set_label (GTK_LABEL (label), g_app_info_get_name (app_info));
  gtk_image_set_from_gicon (GTK_IMAGE (icon), g_app_info_get_icon (app_info));
}

static GtkListItemFactory*
create_app_info_factory (void)
{
  g_autoptr(GtkListItemFactory) factory = NULL;

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (on_signal_item_factory_setup_cb), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (on_signal_item_factory_bind_cb), NULL);

  return g_steal_pointer (&factory);
}

#define OFFSET(x)             (G_STRUCT_OFFSET (CcDefaultAppsPanel, x))
#define WIDGET_FROM_OFFSET(x) (G_STRUCT_MEMBER (GtkWidget*, self, x))

static void
info_panel_setup_default_app (CcDefaultAppsPanel *self,
                              DefaultAppData     *data)
{
  g_autoptr(GtkListItemFactory) factory = NULL;
  g_autoptr(GListModel) apps_list = NULL;
  GtkWidget *row;

  row = adw_combo_row_new ();
  g_object_set_data (G_OBJECT (row), "cc-default-app-data", data);
  adw_preferences_row_set_use_underline (ADW_PREFERENCES_ROW (row), TRUE);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row),
                                 _(data->label));
  adw_preferences_group_add (self->group, row);

  factory = create_app_info_factory ();
  adw_combo_row_set_factory (ADW_COMBO_ROW (row), factory);

  apps_list = get_model_for_content_type (data->content_type);
  adw_combo_row_set_model (ADW_COMBO_ROW (row), apps_list);

  g_signal_connect (row,
                    "notify::selected-item",
                    G_CALLBACK (on_combo_row_selected_item_changedc_cb),
                    self);
}

static DefaultAppData preferred_app_infos[] = {
  { "x-scheme-handler/http", "text/html;application/xhtml+xml;x-scheme-handler/https", N_("_Web") },
  { "x-scheme-handler/mailto", NULL, N_("_Mail") },
  { "text/calendar", NULL, N_("_Calendar") },
  { "audio/x-vorbis+ogg", "audio/*", N_("M_usic") },
  { "video/x-ogm+ogg", "video/*", N_("_Video") },
  { "image/jpeg", "image/*", N_("_Photos") }
};

static void
info_panel_setup_default_apps (CcDefaultAppsPanel *self)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS (preferred_app_infos); i++)
    info_panel_setup_default_app (self, &preferred_app_infos[i]);
}

static void
cc_default_apps_panel_class_init (CcDefaultAppsPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/default-apps/cc-default-apps-panel.ui");
  gtk_widget_class_bind_template_child (widget_class, CcDefaultAppsPanel, group);
}

static void
cc_default_apps_panel_init (CcDefaultAppsPanel *self)
{
  g_resources_register (cc_default_apps_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  info_panel_setup_default_apps (self);
}

GtkWidget *
cc_default_apps_panel_new (void)
{
  return g_object_new (CC_TYPE_DEFAULT_APPS_PANEL,
                       NULL);
}
