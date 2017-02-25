/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
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

#include "cc-info-panel.h"
#include "cc-info-overview-panel.h"
#include "cc-info-resources.h"
#include "info-cleanup.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gio/gunixmounts.h>
#include <gio/gdesktopappinfo.h>

#include <glibtop/fsusage.h>
#include <glibtop/mountlist.h>
#include <glibtop/mem.h>
#include <glibtop/sysinfo.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "gsd-disk-space-helper.h"

/* Autorun options */
#define PREF_MEDIA_AUTORUN_NEVER                "autorun-never"
#define PREF_MEDIA_AUTORUN_X_CONTENT_START_APP  "autorun-x-content-start-app"
#define PREF_MEDIA_AUTORUN_X_CONTENT_IGNORE     "autorun-x-content-ignore"
#define PREF_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER "autorun-x-content-open-folder"

#define CUSTOM_ITEM_ASK "cc-item-ask"
#define CUSTOM_ITEM_DO_NOTHING "cc-item-do-nothing"
#define CUSTOM_ITEM_OPEN_FOLDER "cc-item-open-folder"

#define MEDIA_HANDLING_SCHEMA "org.gnome.desktop.media-handling"

#define WID(w) (GtkWidget *) gtk_builder_get_object (self->priv->builder, w)

CC_PANEL_REGISTER (CcInfoPanel, cc_info_panel)

#define INFO_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_INFO_PANEL, CcInfoPanelPrivate))

typedef struct {
  /* Will be one or 2 GPU name strings, or "Unknown" */
  char *hardware_string;
} GraphicsData;

typedef struct
{
  const char *content_type;
  const char *label;
  /* A pattern used to filter supported mime types
     when changing preferred applications. NULL
     means no other types should be changed */
  const char *extra_type_filter;
} DefaultAppData;

struct _CcInfoPanelPrivate
{
  GtkBuilder    *builder;
  GtkWidget     *extra_options_dialog;
  char          *gnome_version;
  char          *gnome_distributor;
  char          *gnome_date;

  GCancellable  *cancellable;

  /* Free space */
  GList         *primary_mounts;
  guint64        total_bytes;

  /* Media */
  GSettings     *media_settings;
  GtkWidget     *other_application_combo;

  GraphicsData  *graphics_data;
};


static void
graphics_data_free (GraphicsData *gdata)
{
  g_free (gdata->hardware_string);
  g_slice_free (GraphicsData, gdata);
}

static void
cc_info_panel_dispose (GObject *object)
{
  CcInfoPanelPrivate *priv = CC_INFO_PANEL (object)->priv;

  g_clear_object (&priv->builder);
  g_clear_pointer (&priv->graphics_data, graphics_data_free);
  g_clear_pointer (&priv->extra_options_dialog, gtk_widget_destroy);

  G_OBJECT_CLASS (cc_info_panel_parent_class)->dispose (object);
}

static void
cc_info_panel_finalize (GObject *object)
{
  CcInfoPanelPrivate *priv = CC_INFO_PANEL (object)->priv;

  if (priv->cancellable)
    {
      g_cancellable_cancel (priv->cancellable);
      g_clear_object (&priv->cancellable);
    }
  g_free (priv->gnome_version);
  g_free (priv->gnome_date);
  g_free (priv->gnome_distributor);

  g_clear_object (&priv->media_settings);

  G_OBJECT_CLASS (cc_info_panel_parent_class)->finalize (object);
}

static void
cc_info_panel_class_init (CcInfoPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcInfoPanelPrivate));

  object_class->dispose = cc_info_panel_dispose;
  object_class->finalize = cc_info_panel_finalize;

  g_type_ensure (CC_TYPE_INFO_OVERVIEW_PANEL);
}

static void
on_section_changed (GtkTreeSelection  *selection,
                    gpointer           data)
{
  CcInfoPanel *self = CC_INFO_PANEL (data);
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreePath *path;
  gint *indices;
  int index;

  if (!gtk_tree_selection_get_selected (selection, &model, &iter))
    return;

  path = gtk_tree_model_get_path (model, &iter);

  indices = gtk_tree_path_get_indices (path);
  index = indices[0];

  if (index >= 0)
    {
      g_object_set (G_OBJECT (WID ("notebook")),
                    "page", index, NULL);
    }

  gtk_tree_path_free (path);
}

static void
default_app_changed (GtkAppChooserButton *button,
                     CcInfoPanel         *self)
{
  GAppInfo *info;
  GError *error = NULL;
  DefaultAppData *app_data;
  int i;

  info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (button));
  app_data = g_object_get_data (G_OBJECT (button), "cc-default-app-data");

  if (g_app_info_set_as_default_for_type (info, app_data->content_type, &error) == FALSE)
    {
      g_warning ("Failed to set '%s' as the default application for '%s': %s",
                 g_app_info_get_name (info), app_data->content_type, error->message);
      g_error_free (error);
      error = NULL;
    }
  else
    {
      g_debug ("Set '%s' as the default handler for '%s'",
               g_app_info_get_name (info), app_data->content_type);
    }

  if (app_data->extra_type_filter)
    {
      const char *const *mime_types;
      GPatternSpec *pattern;

      pattern = g_pattern_spec_new (app_data->extra_type_filter);
      mime_types = g_app_info_get_supported_types (info);

      for (i = 0; mime_types && mime_types[i]; i++)
        {
          if (!g_pattern_match_string (pattern, mime_types[i]))
            continue;

          if (g_app_info_set_as_default_for_type (info, mime_types[i], &error) == FALSE)
            {
              g_warning ("Failed to set '%s' as the default application for secondary "
                         "content type '%s': %s",
                         g_app_info_get_name (info), mime_types[i], error->message);
              g_error_free (error);
            }
          else
            {
              g_debug ("Set '%s' as the default handler for '%s'",
              g_app_info_get_name (info), mime_types[i]);
            }
        }

      g_pattern_spec_free (pattern);
    }

  g_object_unref (info);
}

static void
info_panel_setup_default_app (CcInfoPanel    *self,
                              DefaultAppData *data,
                              guint           left_attach,
                              guint           top_attach)
{
  GtkWidget *button;
  GtkWidget *grid;
  GtkWidget *label;

  grid = WID ("default_apps_grid");

  button = gtk_app_chooser_button_new (data->content_type);
  g_object_set_data (G_OBJECT (button), "cc-default-app-data", data);

  gtk_app_chooser_button_set_show_default_item (GTK_APP_CHOOSER_BUTTON (button), TRUE);
  gtk_grid_attach (GTK_GRID (grid), button, left_attach, top_attach,
                   1, 1);
  g_signal_connect (G_OBJECT (button), "changed",
                    G_CALLBACK (default_app_changed), self);
  gtk_widget_show (button);

  label = WID(data->label);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), button);
}

static DefaultAppData preferred_app_infos[] = {
  /* for web, we need to support text/html,
     application/xhtml+xml and x-scheme-handler/https,
     hence the "*" pattern
  */
  { "x-scheme-handler/http", "web-label", "*" },
  { "x-scheme-handler/mailto", "mail-label", NULL },
  { "text/calendar", "calendar-label", NULL },
  { "audio/x-vorbis+ogg", "music-label", "audio/*" },
  { "video/x-ogm+ogg", "video-label", "video/*" },
  { "image/jpeg", "photos-label", "image/*" }
};

static void
info_panel_setup_default_apps (CcInfoPanel  *self)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS(preferred_app_infos); i++)
    {
      info_panel_setup_default_app (self, &preferred_app_infos[i],
                                    1, i);
    }
}

static char **
remove_elem_from_str_array (char **v,
                            const char *s)
{
  GPtrArray *array;
  guint idx;

  array = g_ptr_array_new ();

  for (idx = 0; v[idx] != NULL; idx++) {
    if (g_strcmp0 (v[idx], s) == 0) {
      continue;
    }

    g_ptr_array_add (array, v[idx]);
  }

  g_ptr_array_add (array, NULL);

  g_free (v);

  return (char **) g_ptr_array_free (array, FALSE);
}

static char **
add_elem_to_str_array (char **v,
                       const char *s)
{
  GPtrArray *array;
  guint idx;

  array = g_ptr_array_new ();

  for (idx = 0; v[idx] != NULL; idx++) {
    g_ptr_array_add (array, v[idx]);
  }

  g_ptr_array_add (array, g_strdup (s));
  g_ptr_array_add (array, NULL);

  g_free (v);

  return (char **) g_ptr_array_free (array, FALSE);
}

static int
media_panel_g_strv_find (char **strv,
                         const char *find_me)
{
  guint index;

  g_return_val_if_fail (find_me != NULL, -1);

  for (index = 0; strv[index] != NULL; ++index) {
    if (g_strcmp0 (strv[index], find_me) == 0) {
      return index;
    }
  }

  return -1;
}

static void
autorun_get_preferences (CcInfoPanel *self,
                         const char *x_content_type,
                         gboolean *pref_start_app,
                         gboolean *pref_ignore,
                         gboolean *pref_open_folder)
{
  char **x_content_start_app;
  char **x_content_ignore;
  char **x_content_open_folder;

  g_return_if_fail (pref_start_app != NULL);
  g_return_if_fail (pref_ignore != NULL);
  g_return_if_fail (pref_open_folder != NULL);

  *pref_start_app = FALSE;
  *pref_ignore = FALSE;
  *pref_open_folder = FALSE;
  x_content_start_app = g_settings_get_strv (self->priv->media_settings,
                                             PREF_MEDIA_AUTORUN_X_CONTENT_START_APP);
  x_content_ignore = g_settings_get_strv (self->priv->media_settings,
                                          PREF_MEDIA_AUTORUN_X_CONTENT_IGNORE);
  x_content_open_folder = g_settings_get_strv (self->priv->media_settings,
                                               PREF_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER);
  if (x_content_start_app != NULL) {
    *pref_start_app = media_panel_g_strv_find (x_content_start_app, x_content_type) != -1;
  }
  if (x_content_ignore != NULL) {
    *pref_ignore = media_panel_g_strv_find (x_content_ignore, x_content_type) != -1;
  }
  if (x_content_open_folder != NULL) {
    *pref_open_folder = media_panel_g_strv_find (x_content_open_folder, x_content_type) != -1;
  }
  g_strfreev (x_content_ignore);
  g_strfreev (x_content_start_app);
  g_strfreev (x_content_open_folder);
}

static void
autorun_set_preferences (CcInfoPanel *self,
                         const char *x_content_type,
                         gboolean pref_start_app,
                         gboolean pref_ignore,
                         gboolean pref_open_folder)
{
  char **x_content_start_app;
  char **x_content_ignore;
  char **x_content_open_folder;

  g_assert (x_content_type != NULL);

  x_content_start_app = g_settings_get_strv (self->priv->media_settings,
                                             PREF_MEDIA_AUTORUN_X_CONTENT_START_APP);
  x_content_ignore = g_settings_get_strv (self->priv->media_settings,
                                          PREF_MEDIA_AUTORUN_X_CONTENT_IGNORE);
  x_content_open_folder = g_settings_get_strv (self->priv->media_settings,
                                               PREF_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER);

  x_content_start_app = remove_elem_from_str_array (x_content_start_app, x_content_type);
  if (pref_start_app) {
    x_content_start_app = add_elem_to_str_array (x_content_start_app, x_content_type);
  }
  g_settings_set_strv (self->priv->media_settings,
                       PREF_MEDIA_AUTORUN_X_CONTENT_START_APP, (const gchar * const*) x_content_start_app);

  x_content_ignore = remove_elem_from_str_array (x_content_ignore, x_content_type);
  if (pref_ignore) {
    x_content_ignore = add_elem_to_str_array (x_content_ignore, x_content_type);
  }
  g_settings_set_strv (self->priv->media_settings,
                       PREF_MEDIA_AUTORUN_X_CONTENT_IGNORE, (const gchar * const*) x_content_ignore);

  x_content_open_folder = remove_elem_from_str_array (x_content_open_folder, x_content_type);
  if (pref_open_folder) {
    x_content_open_folder = add_elem_to_str_array (x_content_open_folder, x_content_type);
  }
  g_settings_set_strv (self->priv->media_settings,
                       PREF_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER, (const gchar * const*) x_content_open_folder);

  g_strfreev (x_content_open_folder);
  g_strfreev (x_content_ignore);
  g_strfreev (x_content_start_app);

}

static void
custom_item_activated_cb (GtkAppChooserButton *button,
                          const gchar *item,
                          gpointer user_data)
{
  CcInfoPanel *self = user_data;
  gchar *content_type;

  content_type = gtk_app_chooser_get_content_type (GTK_APP_CHOOSER (button));

  if (g_strcmp0 (item, CUSTOM_ITEM_ASK) == 0) {
    autorun_set_preferences (self, content_type,
                             FALSE, FALSE, FALSE);
  } else if (g_strcmp0 (item, CUSTOM_ITEM_OPEN_FOLDER) == 0) {
    autorun_set_preferences (self, content_type,
                             FALSE, FALSE, TRUE);
  } else if (g_strcmp0 (item, CUSTOM_ITEM_DO_NOTHING) == 0) {
    autorun_set_preferences (self, content_type,
                             FALSE, TRUE, FALSE);
  }

  g_free (content_type);
}

static void
combo_box_changed_cb (GtkComboBox *combo_box,
                      gpointer user_data)
{
  CcInfoPanel *self = user_data;
  GAppInfo *info;
  gchar *content_type;

  info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (combo_box));

  if (info == NULL)
    return;

  content_type = gtk_app_chooser_get_content_type (GTK_APP_CHOOSER (combo_box));
  autorun_set_preferences (self, content_type,
                           TRUE, FALSE, FALSE);
  g_app_info_set_as_default_for_type (info, content_type, NULL);

  g_object_unref (info);
  g_free (content_type);
}

static void
prepare_combo_box (CcInfoPanel *self,
                   GtkWidget *combo_box,
                   const gchar *heading)
{
  GtkAppChooserButton *app_chooser = GTK_APP_CHOOSER_BUTTON (combo_box);
  gboolean pref_ask;
  gboolean pref_start_app;
  gboolean pref_ignore;
  gboolean pref_open_folder;
  GAppInfo *info;
  gchar *content_type;

  content_type = gtk_app_chooser_get_content_type (GTK_APP_CHOOSER (app_chooser));

  /* fetch preferences for this content type */
  autorun_get_preferences (self, content_type,
                           &pref_start_app, &pref_ignore, &pref_open_folder);
  pref_ask = !pref_start_app && !pref_ignore && !pref_open_folder;

  info = gtk_app_chooser_get_app_info (GTK_APP_CHOOSER (combo_box));

  /* append the separator only if we have >= 1 apps in the chooser */
  if (info != NULL) {
    gtk_app_chooser_button_append_separator (app_chooser);
    g_object_unref (info);
  }

  gtk_app_chooser_button_append_custom_item (app_chooser, CUSTOM_ITEM_ASK,
                                             _("Ask what to do"),
                                             NULL);

  gtk_app_chooser_button_append_custom_item (app_chooser, CUSTOM_ITEM_DO_NOTHING,
                                             _("Do nothing"),
                                             NULL);

  gtk_app_chooser_button_append_custom_item (app_chooser, CUSTOM_ITEM_OPEN_FOLDER,
                                             _("Open folder"),
                                             NULL);

  gtk_app_chooser_button_set_show_dialog_item (app_chooser, TRUE);
  gtk_app_chooser_button_set_heading (app_chooser, _(heading));

  if (pref_ask) {
    gtk_app_chooser_button_set_active_custom_item (app_chooser, CUSTOM_ITEM_ASK);
  } else if (pref_ignore) {
    gtk_app_chooser_button_set_active_custom_item (app_chooser, CUSTOM_ITEM_DO_NOTHING);
  } else if (pref_open_folder) {
    gtk_app_chooser_button_set_active_custom_item (app_chooser, CUSTOM_ITEM_OPEN_FOLDER);
  }

  g_signal_connect (app_chooser, "changed",
                    G_CALLBACK (combo_box_changed_cb), self);
  g_signal_connect (app_chooser, "custom-item-activated",
                    G_CALLBACK (custom_item_activated_cb), self);

  g_free (content_type);
}

static void
other_type_combo_box_changed (GtkComboBox *combo_box,
                              CcInfoPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  char *x_content_type;
  GtkWidget *action_container;
  GtkWidget *action_label;

  x_content_type = NULL;

  if (!gtk_combo_box_get_active_iter (combo_box, &iter)) {
    return;
  }

  model = gtk_combo_box_get_model (combo_box);
  if (model == NULL) {
    return;
  }

  gtk_tree_model_get (model, &iter,
                      1, &x_content_type,
                      -1);

  action_container = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                                         "media_other_action_container"));
  if (self->priv->other_application_combo != NULL) {
    gtk_widget_destroy (self->priv->other_application_combo);
  }

  self->priv->other_application_combo = gtk_app_chooser_button_new (x_content_type);
  gtk_box_pack_start (GTK_BOX (action_container), self->priv->other_application_combo, TRUE, TRUE, 0);
  prepare_combo_box (self, self->priv->other_application_combo, NULL);
  gtk_widget_show (self->priv->other_application_combo);

  action_label = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
                                                     "media_other_action_label"));

  gtk_label_set_mnemonic_widget (GTK_LABEL (action_label), self->priv->other_application_combo);

  g_free (x_content_type);
}

static void
on_extra_options_dialog_response (GtkWidget    *dialog,
                                  int           response,
                                  CcInfoPanel *self)
{
  gtk_widget_hide (dialog);

  if (self->priv->other_application_combo != NULL) {
    gtk_widget_destroy (self->priv->other_application_combo);
    self->priv->other_application_combo = NULL;
  }
}

static void
on_extra_options_button_clicked (GtkWidget    *button,
                                 CcInfoPanel *self)
{
  GtkWidget *dialog;
  GtkWidget *combo_box;

  dialog = self->priv->extra_options_dialog;
  combo_box = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "media_other_type_combobox"));
  gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_title (GTK_WINDOW (dialog), _("Other Media"));
  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (on_extra_options_dialog_response),
                    self);
  g_signal_connect (dialog,
                    "delete-event",
                    G_CALLBACK (gtk_widget_hide_on_delete),
                    NULL);
  /* update other_application_combo */
  other_type_combo_box_changed (GTK_COMBO_BOX (combo_box), self);
  gtk_window_present (GTK_WINDOW (dialog));
}

static void
info_panel_setup_media (CcInfoPanel *self)
{
  guint n;
  GList *l, *content_types;
  GtkWidget *other_type_combo_box;
  GtkWidget *extras_button;
  GtkListStore *other_type_list_store;
  GtkCellRenderer *renderer;
  GtkTreeIter iter;
  GtkBuilder *builder = self->priv->builder;

  struct {
    const gchar *widget_name;
    const gchar *content_type;
    const gchar *heading;
  } const defs[] = {
    { "media_audio_cdda_combobox", "x-content/audio-cdda", N_("Select an application for audio CDs") },
    { "media_video_dvd_combobox", "x-content/video-dvd", N_("Select an application for video DVDs") },
    { "media_music_player_combobox", "x-content/audio-player", N_("Select an application to run when a music player is connected") },
    { "media_dcf_combobox", "x-content/image-dcf", N_("Select an application to run when a camera is connected") },
    { "media_software_combobox", "x-content/unix-software", N_("Select an application for software CDs") },
  };

  struct {
    const gchar *content_type;
    const gchar *description;
  } const other_defs[] = {
    /* translators: these strings are duplicates of shared-mime-info
     * strings, just here to fix capitalization of the English originals.
     * If the shared-mime-info translation works for your language,
     * simply leave these untranslated.
     */
    { "x-content/audio-dvd", N_("audio DVD") },
    { "x-content/blank-bd", N_("blank Blu-ray disc") },
    { "x-content/blank-cd", N_("blank CD disc") },
    { "x-content/blank-dvd", N_("blank DVD disc") },
    { "x-content/blank-hddvd", N_("blank HD DVD disc") },
    { "x-content/video-bluray", N_("Blu-ray video disc") },
    { "x-content/ebook-reader", N_("e-book reader") },
    { "x-content/video-hddvd", N_("HD DVD video disc") },
    { "x-content/image-picturecd", N_("Picture CD") },
    { "x-content/video-svcd", N_("Super Video CD") },
    { "x-content/video-vcd", N_("Video CD") },
    { "x-content/win32-software", N_("Windows software") },
  };

  for (n = 0; n < G_N_ELEMENTS (defs); n++) {
    prepare_combo_box (self,
                       GTK_WIDGET (gtk_builder_get_object (builder, defs[n].widget_name)),
                       defs[n].heading);
  }

  other_type_combo_box = GTK_WIDGET (gtk_builder_get_object (builder, "media_other_type_combobox"));

  other_type_list_store = gtk_list_store_new (2,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (other_type_list_store),
                                        1, GTK_SORT_ASCENDING);


  content_types = g_content_types_get_registered ();

  for (l = content_types; l != NULL; l = l->next) {
    char *content_type = l->data;
    char *description = NULL;

    if (!g_str_has_prefix (content_type, "x-content/"))
      continue;

    for (n = 0; n < G_N_ELEMENTS (defs); n++) {
      if (g_content_type_is_a (content_type, defs[n].content_type)) {
        goto skip;
      }
    }

    for (n = 0; n < G_N_ELEMENTS (other_defs); n++) {
       if (strcmp (content_type, other_defs[n].content_type) == 0) {
         const gchar *s = other_defs[n].description;
         if (s == _(s))
           description = g_content_type_get_description (content_type);
         else
           description = g_strdup (_(s));

         break;
       }
    }

    if (description == NULL) {
      g_debug ("Content type '%s' is missing from the info panel", content_type);
      description = g_content_type_get_description (content_type);
    }

    gtk_list_store_append (other_type_list_store, &iter);

    gtk_list_store_set (other_type_list_store, &iter,
                        0, description,
                        1, content_type,
                        -1);
    g_free (description);
  skip:
    ;
  }

  g_list_free_full (content_types, g_free);

  gtk_combo_box_set_model (GTK_COMBO_BOX (other_type_combo_box),
                           GTK_TREE_MODEL (other_type_list_store));

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (other_type_combo_box), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (other_type_combo_box), renderer,
                                  "text", 0,
                                  NULL);

  g_signal_connect (other_type_combo_box,
                    "changed",
                    G_CALLBACK (other_type_combo_box_changed),
                    self);

  gtk_combo_box_set_active (GTK_COMBO_BOX (other_type_combo_box), 0);

  extras_button = GTK_WIDGET (gtk_builder_get_object (builder, "extra_options_button"));
  g_signal_connect (extras_button,
                    "clicked",
                    G_CALLBACK (on_extra_options_button_clicked),
                    self);

  g_settings_bind (self->priv->media_settings,
                   PREF_MEDIA_AUTORUN_NEVER,
                   gtk_builder_get_object (self->priv->builder, "media_autorun_never_checkbutton"),
                   "active",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_bind (self->priv->media_settings,
                   PREF_MEDIA_AUTORUN_NEVER,
                   GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "media_handling_vbox")),
                   "sensitive",
                   G_SETTINGS_BIND_INVERT_BOOLEAN);
}

static void
info_panel_setup_selector (CcInfoPanel  *self)
{
  GtkTreeView *view;
  GtkListStore *model;
  GtkTreeSelection *selection;
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkTreeIter iter;
  int section_name_column = 0;

  view = GTK_TREE_VIEW (WID ("overview_treeview"));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  model = gtk_list_store_new (1, G_TYPE_STRING);
  gtk_tree_view_set_model (view, GTK_TREE_MODEL (model));
  g_object_unref (model);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_renderer_set_padding (renderer, 4, 4);
  g_object_set (renderer,
                "width-chars", 20,
                "ellipsize", PANGO_ELLIPSIZE_END,
                NULL);
  column = gtk_tree_view_column_new_with_attributes (_("Section"),
                                                     renderer,
                                                     "text", section_name_column,
                                                     NULL);
  gtk_tree_view_append_column (view, column);


  gtk_list_store_append (model, &iter);
  gtk_list_store_set (model, &iter, section_name_column,
                      _("Overview"),
                      -1);
  gtk_tree_selection_select_iter (selection, &iter);

  gtk_list_store_append (model, &iter);
  gtk_list_store_set (model, &iter, section_name_column,
                      _("Default Applications"),
                      -1);

  gtk_list_store_append (model, &iter);
  gtk_list_store_set (model, &iter, section_name_column,
                      _("Removable Media"),
                      -1);

  g_signal_connect (selection, "changed",
                    G_CALLBACK (on_section_changed), self);
  on_section_changed (selection, self);

  gtk_widget_show_all (GTK_WIDGET (view));
}

static void
info_panel_setup_overview (CcInfoPanel  *self)
{
  GtkWidget  *widget;

  widget = WID ("info_vbox");
  gtk_container_add (GTK_CONTAINER (self), widget);
}

static void
cc_info_panel_init (CcInfoPanel *self)
{
  GError *error = NULL;

  self->priv = INFO_PANEL_PRIVATE (self);
  g_resources_register (cc_info_get_resource ());

  self->priv->builder = gtk_builder_new ();

  self->priv->media_settings = g_settings_new (MEDIA_HANDLING_SCHEMA);

  if (gtk_builder_add_from_resource (self->priv->builder,
                                     "/org/gnome/control-center/info/info.ui",
                                     &error) == 0)
    {
      g_warning ("Could not load interface file: %s", error->message);
      g_error_free (error);
      return;
    }

  self->priv->extra_options_dialog = WID ("extra_options_dialog");

  info_panel_setup_selector (self);
  info_panel_setup_overview (self);
  info_panel_setup_default_apps (self);
  info_panel_setup_media (self);
}
