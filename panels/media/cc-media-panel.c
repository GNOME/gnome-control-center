/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2008-2010 Red Hat, Inc.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Author: David Zeuthen <davidz@redhat.com>
 *         Cosimo Cecchi <cosimoc@gnome.org>
 *
 */

#include <config.h>

#include "cc-media-panel.h"

#include <string.h>
#include <glib/gi18n.h>

G_DEFINE_DYNAMIC_TYPE (CcMediaPanel, cc_media_panel, CC_TYPE_PANEL)

#define MEDIA_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_MEDIA_PANEL, CcMediaPanelPrivate))

/* Autorun options */
#define PREF_MEDIA_AUTORUN_NEVER                "autorun-never"
#define PREF_MEDIA_AUTORUN_X_CONTENT_START_APP  "autorun-x-content-start-app"
#define PREF_MEDIA_AUTORUN_X_CONTENT_IGNORE     "autorun-x-content-ignore"
#define PREF_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER "autorun-x-content-open-folder"

#define CUSTOM_ITEM_ASK "cc-item-ask"
#define CUSTOM_ITEM_DO_NOTHING "cc-item-do-nothing"
#define CUSTOM_ITEM_OPEN_FOLDER "cc-item-open-folder"

struct _CcMediaPanelPrivate {
  GtkBuilder *builder;
  GSettings *preferences;

  GtkWidget *other_application_combo;
};

static void
cc_media_panel_dispose (GObject *object)
{
  CcMediaPanel *self = CC_MEDIA_PANEL (object);

  if (self->priv->builder != NULL) {
    g_object_unref (self->priv->builder);
    self->priv->builder = NULL;
  }

  if (self->priv->preferences != NULL) {
    g_object_unref (self->priv->preferences);
    self->priv->preferences = NULL;
  }

  G_OBJECT_CLASS (cc_media_panel_parent_class)->dispose (object);
}

static void
cc_media_panel_finalize (GObject *object)
{
  G_OBJECT_CLASS (cc_media_panel_parent_class)->finalize (object);
}

static void
cc_media_panel_class_init (CcMediaPanelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (CcMediaPanelPrivate));

  object_class->dispose = cc_media_panel_dispose;
  object_class->finalize = cc_media_panel_finalize;
}

static void
cc_media_panel_class_finalize (CcMediaPanelClass *klass)
{
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
autorun_get_preferences (CcMediaPanel *self,
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
  x_content_start_app = g_settings_get_strv (self->priv->preferences,
                                             PREF_MEDIA_AUTORUN_X_CONTENT_START_APP);
  x_content_ignore = g_settings_get_strv (self->priv->preferences,
                                          PREF_MEDIA_AUTORUN_X_CONTENT_IGNORE);
  x_content_open_folder = g_settings_get_strv (self->priv->preferences,
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
autorun_set_preferences (CcMediaPanel *self,
                         const char *x_content_type,
                         gboolean pref_start_app,
                         gboolean pref_ignore,
                         gboolean pref_open_folder)
{
  char **x_content_start_app;
  char **x_content_ignore;
  char **x_content_open_folder;

  g_assert (x_content_type != NULL);

  x_content_start_app = g_settings_get_strv (self->priv->preferences,
                                             PREF_MEDIA_AUTORUN_X_CONTENT_START_APP);
  x_content_ignore = g_settings_get_strv (self->priv->preferences,
                                          PREF_MEDIA_AUTORUN_X_CONTENT_IGNORE);
  x_content_open_folder = g_settings_get_strv (self->priv->preferences,
                                               PREF_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER);

  x_content_start_app = remove_elem_from_str_array (x_content_start_app, x_content_type);
  if (pref_start_app) {
    x_content_start_app = add_elem_to_str_array (x_content_start_app, x_content_type);
  }
  g_settings_set_strv (self->priv->preferences,
                       PREF_MEDIA_AUTORUN_X_CONTENT_START_APP, (const gchar * const*) x_content_start_app);

  x_content_ignore = remove_elem_from_str_array (x_content_ignore, x_content_type);
  if (pref_ignore) {
    x_content_ignore = add_elem_to_str_array (x_content_ignore, x_content_type);
  }
  g_settings_set_strv (self->priv->preferences,
                       PREF_MEDIA_AUTORUN_X_CONTENT_IGNORE, (const gchar * const*) x_content_ignore);

  x_content_open_folder = remove_elem_from_str_array (x_content_open_folder, x_content_type);
  if (pref_open_folder) {
    x_content_open_folder = add_elem_to_str_array (x_content_open_folder, x_content_type);
  }
  g_settings_set_strv (self->priv->preferences,
                       PREF_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER, (const gchar * const*) x_content_open_folder);

  g_strfreev (x_content_open_folder);
  g_strfreev (x_content_ignore);
  g_strfreev (x_content_start_app);

}

static void
update_media_sensitivity (CcMediaPanel *self)
{
  gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "media_handling_vbox")),
                            ! g_settings_get_boolean (self->priv->preferences, PREF_MEDIA_AUTORUN_NEVER));
}

static void
custom_item_activated_cb (GtkAppChooserButton *button,
                          const gchar *item,
                          gpointer user_data)
{
  CcMediaPanel *self = user_data;
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
  CcMediaPanel *self = user_data;
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
prepare_combo_box (CcMediaPanel *self,
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
                              CcMediaPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  char *x_content_type;
  GtkWidget *action_container;

  x_content_type = NULL;

  if (!gtk_combo_box_get_active_iter (combo_box, &iter)) {
    return;
  }

  model = gtk_combo_box_get_model (combo_box);
  if (model == NULL) {
    return;
  }

  gtk_tree_model_get (model, &iter,
                      2, &x_content_type,
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

  g_free (x_content_type);
}

static void
on_extra_options_dialog_response (GtkWidget    *dialog,
                                  int           response,
                                  CcMediaPanel *self)
{
  gtk_widget_hide (dialog);

  if (self->priv->other_application_combo != NULL) {
    gtk_widget_destroy (self->priv->other_application_combo);
    self->priv->other_application_combo = NULL;
  }
}

static void
on_extra_options_button_clicked (GtkWidget    *button,
                                 CcMediaPanel *self)
{
  GtkWidget *dialog;
  GtkWidget *combo_box;

  dialog = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "extra_options_dialog"));
  combo_box = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "media_other_type_combobox"));
  gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (on_extra_options_dialog_response),
                    self);
  /* update other_application_combo */
  other_type_combo_box_changed (GTK_COMBO_BOX (combo_box), self);
  gtk_window_present (GTK_WINDOW (dialog));
}

static void
media_panel_setup (CcMediaPanel *self)
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
    { "media_software_combobox", "x-content/software", N_("Select an application for software CDs") },
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
    { "x-content/video-vcd", N_("Video CD") }
  };

  for (n = 0; n < G_N_ELEMENTS (defs); n++) {
    prepare_combo_box (self,
                       GTK_WIDGET (gtk_builder_get_object (builder, defs[n].widget_name)),
                       defs[n].heading);
  }

  other_type_combo_box = GTK_WIDGET (gtk_builder_get_object (builder, "media_other_type_combobox"));

  other_type_list_store = gtk_list_store_new (3,
                                              G_TYPE_ICON,
                                              G_TYPE_STRING,
                                              G_TYPE_STRING);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (other_type_list_store),
                                        1, GTK_SORT_ASCENDING);


  content_types = g_content_types_get_registered ();

  for (l = content_types; l != NULL; l = l->next) {
    char *content_type = l->data;
    char *description = NULL;
    GIcon *icon;

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

    gtk_list_store_append (other_type_list_store, &iter);
    icon = g_content_type_get_icon (content_type);

    gtk_list_store_set (other_type_list_store, &iter,
                        0, icon,
                        1, description,
                        2, content_type,
                        -1);
    g_free (description);
    g_object_unref (icon);
  skip:
    ;
  }

  g_list_free_full (content_types, g_free);

  gtk_combo_box_set_model (GTK_COMBO_BOX (other_type_combo_box),
                           GTK_TREE_MODEL (other_type_list_store));

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (other_type_combo_box), renderer, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (other_type_combo_box), renderer,
                                  "gicon", 0,
                                  NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (other_type_combo_box), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (other_type_combo_box), renderer,
                                  "text", 1,
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

  update_media_sensitivity (self);
}

static void
cc_media_panel_init (CcMediaPanel *self)
{
  CcMediaPanelPrivate *priv;
  GtkWidget *main_widget;
  guint res;

  priv = self->priv = MEDIA_PANEL_PRIVATE (self);
  priv->builder = gtk_builder_new ();
  priv->preferences = g_settings_new ("org.gnome.desktop.media-handling");

  res = gtk_builder_add_from_file (priv->builder, GNOMECC_UI_DIR "/gnome-media-properties.ui", NULL);

  /* TODO: error */
  if (res == 0) {
    g_critical ("Unable to load the UI file!");
  }

  media_panel_setup (self);

  main_widget = GTK_WIDGET (gtk_builder_get_object (priv->builder,
                                                    "media_preferences_vbox"));
  gtk_widget_reparent (main_widget, (GtkWidget *) self);
}

void
cc_media_panel_register (GIOModule *module)
{
  cc_media_panel_register_type (G_TYPE_MODULE (module));
  g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
                                  CC_TYPE_MEDIA_PANEL,
                                  "media", 0);
}

