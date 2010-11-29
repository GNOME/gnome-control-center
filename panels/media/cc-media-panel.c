/*
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

#include "nautilus-open-with-dialog.h"

G_DEFINE_DYNAMIC_TYPE (CcMediaPanel, cc_media_panel, CC_TYPE_PANEL)

#define MEDIA_PANEL_PRIVATE(o) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_MEDIA_PANEL, CcMediaPanelPrivate))

/* Autorun options */
#define PREF_MEDIA_AUTORUN_NEVER                "autorun-never"
#define PREF_MEDIA_AUTORUN_X_CONTENT_START_APP  "autorun-x-content-start-app"
#define PREF_MEDIA_AUTORUN_X_CONTENT_IGNORE     "autorun-x-content-ignore"
#define PREF_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER "autorun-x-content-open-folder"

enum {
  AUTORUN_ASK,
  AUTORUN_IGNORE,
  AUTORUN_APP,
  AUTORUN_OPEN_FOLDER,
  AUTORUN_SEP,
  AUTORUN_OTHER_APP,
};

enum {
  COLUMN_AUTORUN_GICON,
  COLUMN_AUTORUN_NAME,
  COLUMN_AUTORUN_APP_INFO,
  COLUMN_AUTORUN_X_CONTENT_TYPE,
  COLUMN_AUTORUN_ITEM_TYPE,
};

enum {
  COMBO_ITEM_ASK_OR_LABEL = 0,
  COMBO_ITEM_DO_NOTHING,
  COMBO_ITEM_OPEN_FOLDER
};

struct _CcMediaPanelPrivate {
  GtkBuilder *builder;
  GSettings *preferences;
};

typedef struct {
  guint changed_signal_id;
  GtkWidget *combo_box;

  char *x_content_type;

  gboolean other_application_selected;
  CcMediaPanel *self;
} AutorunComboBoxData;

static void
prepare_combo_box (CcMediaPanel *self,
		   GtkWidget *combo_box,
		   const char *x_content_type);

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

static void
update_media_sensitivity (CcMediaPanel *self)
{
  gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "media_handling_vbox")),
			    ! g_settings_get_boolean (self->priv->preferences, PREF_MEDIA_AUTORUN_NEVER));
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
autorun_rebuild_combo_box (AutorunComboBoxData *data)
{
  char *x_content_type;

  x_content_type = g_strdup (data->x_content_type);
  prepare_combo_box (data->self,
		     data->combo_box,
		     x_content_type);

  g_free (x_content_type);
}

static void
other_application_selected (NautilusOpenWithDialog *dialog,
			    GAppInfo *app_info,
			    AutorunComboBoxData *data)
{
  autorun_set_preferences (data->self, data->x_content_type, TRUE, FALSE, FALSE);
  g_app_info_set_as_default_for_type (app_info,
				      data->x_content_type,
				      NULL);
  data->other_application_selected = TRUE;

  /* rebuild so we include and select the new application in the list */
  autorun_rebuild_combo_box (data);
}

static void
handle_dialog_closure (AutorunComboBoxData *data)
{
  if (!data->other_application_selected) {
    /* reset combo box so we don't linger on "Open with other Application..." */
    autorun_rebuild_combo_box (data);
  }
}

static void 
combo_box_changed (GtkComboBox *combo_box,
                   AutorunComboBoxData *data)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GAppInfo *app_info;
  char *x_content_type;
  int type;
  CcMediaPanel *self = data->self;

  model = NULL;
  app_info = NULL;
  x_content_type = NULL;

  if (!gtk_combo_box_get_active_iter (combo_box, &iter)) {
    goto out;
  }

  model = gtk_combo_box_get_model (combo_box);
  if (model == NULL) {
    goto out;
  }

  gtk_tree_model_get (model, &iter, 
		      COLUMN_AUTORUN_APP_INFO, &app_info,
		      COLUMN_AUTORUN_X_CONTENT_TYPE, &x_content_type,
		      COLUMN_AUTORUN_ITEM_TYPE, &type,
		      -1);

  switch (type) {
  case AUTORUN_ASK:
    autorun_set_preferences (self, x_content_type,
			     FALSE, FALSE, FALSE);
    break;
  case AUTORUN_IGNORE:
    autorun_set_preferences (self, x_content_type,
			     FALSE, TRUE, FALSE);
    break;
  case AUTORUN_OPEN_FOLDER:
    autorun_set_preferences (self, x_content_type,
			     FALSE, FALSE, TRUE);
    break;

  case AUTORUN_APP:
    autorun_set_preferences (self, x_content_type,
			     TRUE, FALSE, FALSE);
    g_app_info_set_as_default_for_type (app_info,
					x_content_type,
					NULL);
    break;

  case AUTORUN_OTHER_APP:
    {
      GtkWidget *dialog;

      data->other_application_selected = FALSE;

      dialog = nautilus_add_application_dialog_new (NULL, x_content_type);
      gtk_window_set_transient_for (GTK_WINDOW (dialog),
				    GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (data->self))));
      gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
      g_signal_connect (dialog, "application_selected",
			G_CALLBACK (other_application_selected),
			data);
      g_signal_connect_swapped (dialog, "response",
				G_CALLBACK (handle_dialog_closure), data);
      g_signal_connect_swapped (dialog, "destroy",
				G_CALLBACK (handle_dialog_closure), data);
      gtk_widget_show (GTK_WIDGET (dialog));

      break;
    }
  }
 
 out:
  if (app_info != NULL) {
    g_object_unref (app_info);
  }

  g_free (x_content_type);
}

static void 
other_type_combo_box_changed (GtkComboBox *combo_box,
			      CcMediaPanel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  char *x_content_type;
  GtkWidget *action_combo_box;

  x_content_type = NULL;

  if (!gtk_combo_box_get_active_iter (combo_box, &iter)) {
    goto out;
  }

  model = gtk_combo_box_get_model (combo_box);
  if (model == NULL) {
    goto out;
  }

  gtk_tree_model_get (model, &iter, 
		      2, &x_content_type,
		      -1);

  action_combo_box = GTK_WIDGET (gtk_builder_get_object (self->priv->builder,
							 "media_other_action_combobox"));

  prepare_combo_box (self, action_combo_box,
		     x_content_type);
 out:
  g_free (x_content_type);
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

static gboolean
combo_box_separator_func (GtkTreeModel *model,
                          GtkTreeIter *iter,
                          gpointer data)
{
  char *str;

  gtk_tree_model_get (model, iter,
		      1, &str,
		      -1);

  if (str != NULL) {
    g_free (str);
    return FALSE;
  }

  return TRUE;
}

static void 
autorun_combobox_data_destroy (AutorunComboBoxData *data)
{
  /* signal handler may be automatically disconnected by destroying the widget */
  if (g_signal_handler_is_connected (data->combo_box, data->changed_signal_id)) {
    g_signal_handler_disconnect (data->combo_box, data->changed_signal_id);
  }
  g_free (data->x_content_type);
  g_free (data);
}

static void
prepare_combo_box (CcMediaPanel *self,
		   GtkWidget *combo_box,
		   const char *x_content_type)
{
  GList *l;
  GList *app_info_list;
  GAppInfo *default_app_info;
  GtkListStore *list_store;
  GtkTreeIter iter;
  GIcon *icon;
  int set_active;
  int n;
  int num_apps;
  gboolean pref_ask;
  gboolean pref_start_app;
  gboolean pref_ignore;
  gboolean pref_open_folder;
  AutorunComboBoxData *data;
  GtkCellRenderer *renderer;
  gboolean new_data;

  autorun_get_preferences (self, x_content_type,
			   &pref_start_app, &pref_ignore, &pref_open_folder);
  pref_ask = !pref_start_app && !pref_ignore && !pref_open_folder;

  set_active = -1;
  data = NULL;
  new_data = TRUE;

  app_info_list = g_app_info_get_all_for_type (x_content_type);
  default_app_info = g_app_info_get_default_for_type (x_content_type, FALSE);
  num_apps = g_list_length (app_info_list);

  list_store = gtk_list_store_new (5,
				   G_TYPE_ICON,
				   G_TYPE_STRING,
				   G_TYPE_APP_INFO,
				   G_TYPE_STRING,
				   G_TYPE_INT);

  /* no apps installed */
  if (num_apps == 0) {
    gtk_list_store_append (list_store, &iter);
    icon = g_themed_icon_new (GTK_STOCK_DIALOG_ERROR);

    /* TODO: integrate with PackageKit-gnome to find applications */

    gtk_list_store_set (list_store, &iter,
			COLUMN_AUTORUN_GICON, icon,
			COLUMN_AUTORUN_NAME, _("No applications found"),
			COLUMN_AUTORUN_APP_INFO, NULL,
			COLUMN_AUTORUN_X_CONTENT_TYPE, x_content_type,
			COLUMN_AUTORUN_ITEM_TYPE, AUTORUN_ASK,
			-1);
    g_object_unref (icon);
  } else {
    gtk_list_store_append (list_store, &iter);
    icon = g_themed_icon_new (GTK_STOCK_DIALOG_QUESTION);

    gtk_list_store_set (list_store, &iter, 
			COLUMN_AUTORUN_GICON, icon, 
			COLUMN_AUTORUN_NAME, _("Ask what to do"), 
			COLUMN_AUTORUN_APP_INFO, NULL, 
			COLUMN_AUTORUN_X_CONTENT_TYPE, x_content_type,
			COLUMN_AUTORUN_ITEM_TYPE, AUTORUN_ASK,
			-1);
    g_object_unref (icon);

    gtk_list_store_append (list_store, &iter);
    icon = g_themed_icon_new (GTK_STOCK_CLOSE);

    gtk_list_store_set (list_store, &iter, 
			COLUMN_AUTORUN_GICON, icon,
			COLUMN_AUTORUN_NAME, _("Do Nothing"), 
			COLUMN_AUTORUN_APP_INFO, NULL, 
			COLUMN_AUTORUN_X_CONTENT_TYPE, x_content_type,
			COLUMN_AUTORUN_ITEM_TYPE, AUTORUN_IGNORE,
			-1);
    g_object_unref (icon);

    gtk_list_store_append (list_store, &iter);
    icon = g_themed_icon_new ("folder-open");

    gtk_list_store_set (list_store, &iter, 
			COLUMN_AUTORUN_GICON, icon,
			COLUMN_AUTORUN_NAME, _("Open Folder"), 
			COLUMN_AUTORUN_APP_INFO, NULL, 
			COLUMN_AUTORUN_X_CONTENT_TYPE, x_content_type,
			COLUMN_AUTORUN_ITEM_TYPE, AUTORUN_OPEN_FOLDER,
			-1);
    g_object_unref (icon);	

    gtk_list_store_append (list_store, &iter);
    gtk_list_store_set (list_store, &iter, 
			COLUMN_AUTORUN_GICON, NULL, 
			COLUMN_AUTORUN_NAME, NULL, 
			COLUMN_AUTORUN_APP_INFO, NULL, 
			COLUMN_AUTORUN_X_CONTENT_TYPE, NULL,
			COLUMN_AUTORUN_ITEM_TYPE, AUTORUN_SEP,
			-1);

    for (l = app_info_list, n = 4; l != NULL; l = l->next, n++) {
      char *open_string;
      GAppInfo *app_info = l->data;
			
      /* we deliberately ignore should_show because some apps might want
       * to install special handlers that should be hidden in the regular
       * application launcher menus
       */
			
      icon = g_app_info_get_icon (app_info);
      open_string = g_strdup_printf (_("Open %s"), g_app_info_get_display_name (app_info));

      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (list_store, &iter, 
			  COLUMN_AUTORUN_GICON, icon,
			  COLUMN_AUTORUN_NAME, open_string, 
			  COLUMN_AUTORUN_APP_INFO, app_info, 
			  COLUMN_AUTORUN_X_CONTENT_TYPE, x_content_type,
			  COLUMN_AUTORUN_ITEM_TYPE, AUTORUN_APP,
			  -1);

      g_free (open_string);
			
      if (g_app_info_equal (app_info, default_app_info)) {
	set_active = n;
      }
    }
  }

  gtk_list_store_append (list_store, &iter);
  gtk_list_store_set (list_store, &iter,
		      COLUMN_AUTORUN_GICON, NULL,
		      COLUMN_AUTORUN_NAME, NULL,
		      COLUMN_AUTORUN_APP_INFO, NULL,
		      COLUMN_AUTORUN_X_CONTENT_TYPE, NULL,
		      COLUMN_AUTORUN_ITEM_TYPE, AUTORUN_SEP,
		      -1);

  gtk_list_store_append (list_store, &iter);
  icon = g_themed_icon_new ("application-x-executable");

  gtk_list_store_set (list_store, &iter,
		      COLUMN_AUTORUN_GICON, icon,
		      COLUMN_AUTORUN_NAME, _("Open with other Application..."),
		      COLUMN_AUTORUN_APP_INFO, NULL,
		      COLUMN_AUTORUN_X_CONTENT_TYPE, x_content_type,
		      COLUMN_AUTORUN_ITEM_TYPE, AUTORUN_OTHER_APP,
		      -1);
  g_object_unref (icon);

  if (default_app_info != NULL) {
    g_object_unref (default_app_info);
  }
  g_list_free_full (app_info_list, g_object_unref);

  gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box), GTK_TREE_MODEL (list_store));
  g_object_unref (list_store);

  gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo_box));

  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
				  "gicon", COLUMN_AUTORUN_GICON,
				  NULL);
  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo_box), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo_box), renderer,
				  "text", COLUMN_AUTORUN_NAME,
				  NULL);
  gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (combo_box), combo_box_separator_func, NULL, NULL);

  if (num_apps == 0) {
    gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), COMBO_ITEM_ASK_OR_LABEL);
    gtk_widget_set_sensitive (combo_box, FALSE);
  } else {
    gtk_widget_set_sensitive (combo_box, TRUE);
    if (pref_ask) {
      gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), COMBO_ITEM_ASK_OR_LABEL);
    } else if (pref_ignore) {
      gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), COMBO_ITEM_DO_NOTHING);
    } else if (pref_open_folder) {
      gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), COMBO_ITEM_OPEN_FOLDER);
    } else if (set_active != -1) {
      gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), set_active);
    } else {
      gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box), COMBO_ITEM_DO_NOTHING);
    }

    /* See if we have an old data around */
    data = g_object_get_data (G_OBJECT (combo_box), "autorun_combobox_data");
    if (data) {
      new_data = FALSE;
      g_free (data->x_content_type);
    } else {
      data = g_new0 (AutorunComboBoxData, 1);
    }
	
    data->x_content_type = g_strdup (x_content_type);
    data->combo_box = combo_box;
    data->self = self;

    if (data->changed_signal_id == 0) {
      data->changed_signal_id = g_signal_connect (combo_box,
						  "changed",
						  G_CALLBACK (combo_box_changed),
						  data);
    }
  }

  if (new_data) {
    g_object_set_data_full (G_OBJECT (combo_box),
			    "autorun_combobox_data",
			    data,
			    (GDestroyNotify) autorun_combobox_data_destroy);
  }
}

static void
on_extra_options_dialog_response (GtkWidget    *dialog,
                                  int           response,
                                  CcMediaPanel *self)
{
  gtk_widget_hide (dialog);
}

static void
on_extra_options_button_clicked (GtkWidget    *button,
                                 CcMediaPanel *self)
{
  GtkWidget  *dialog;

  dialog = GTK_WIDGET (gtk_builder_get_object (self->priv->builder, "extra_options_dialog"));
  gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))));
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (on_extra_options_dialog_response),
                    NULL);
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
  } const defs[] = {
    { "media_audio_cdda_combobox", "x-content/audio-cdda" },
    { "media_video_dvd_combobox", "x-content/video-dvd" },
    { "media_music_player_combobox", "x-content/audio-player" },
    { "media_dcf_combobox", "x-content/image-dcf" },
    { "media_software_combobox", "x-content/software" },
  };

  for (n = 0; n < G_N_ELEMENTS (defs); n++) {
    prepare_combo_box (self,
		       GTK_WIDGET (gtk_builder_get_object (builder, defs[n].widget_name)),
		       defs[n].content_type);
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
    char *description;
    GIcon *icon;

    if (!g_str_has_prefix (content_type, "x-content/"))
      continue;

    for (n = 0; n < G_N_ELEMENTS (defs); n++) {
      if (g_content_type_is_a (content_type, defs[n].content_type)) {
	goto skip;
      }
    }

    description = g_content_type_get_description (content_type);
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

