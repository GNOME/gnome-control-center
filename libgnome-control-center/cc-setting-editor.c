/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2010 Rodrigo Moya
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
 * Authors: Rodrigo Moya <rodrigo@gnome.org>
 */

#include "cc-setting-editor.h"
#include "cc-marshal.h"

enum {
  PIXBUF_COL,
  TEXT_COL,
  APP_INFO_COL,
  N_COLUMNS
};

enum {
  VALUE_CHANGED,
  LAST_SIGNAL
};

typedef enum {
  SETTING_TYPE_GSETTINGS,
  SETTING_TYPE_APPLICATION
} SettingType;

struct _CcSettingEditorPrivate
{
  SettingType s_type;

  GSettings *settings;
  gchar *settings_prefix;
  gchar *key;
  GtkWidget *ui_control;
};

static guint seditor_signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE(CcSettingEditor, cc_setting_editor, GTK_TYPE_VBOX)

static void
cc_setting_editor_finalize (GObject *object)
{
  CcSettingEditor *seditor = CC_SETTING_EDITOR (object);

  if (seditor->priv != NULL) {
    if (seditor->priv->settings != NULL)
      g_object_unref (seditor->priv->settings);

    if (seditor->priv->settings_prefix != NULL)
      g_free (seditor->priv->settings_prefix);

    if (seditor->priv->key != NULL)
      g_free (seditor->priv->key);

    g_free (seditor->priv);
  }

  G_OBJECT_CLASS (cc_setting_editor_parent_class)->finalize (object);
}

static void
cc_setting_editor_class_init (CcSettingEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_setting_editor_finalize;

  /* Class signals */
  seditor_signals[VALUE_CHANGED] = g_signal_new ("value-changed",
                                                 G_TYPE_FROM_CLASS (klass), 0,
                                                 G_STRUCT_OFFSET (CcSettingEditorClass, value_changed),
                                                 NULL, NULL,
                                                 (GSignalCMarshaller) cc_marshal_VOID__STRING_POINTER,
                                                 G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_POINTER);
}

static void
cc_setting_editor_init (CcSettingEditor *seditor)
{
  seditor->priv = g_new0 (CcSettingEditorPrivate, 1);
}

static CcSettingEditor *
cc_setting_editor_new (SettingType s_type,
                       const gchar *settings_prefix,
                       const gchar *key,
                       GtkWidget *ui_control)
{
  CcSettingEditor *seditor;

  seditor = g_object_new (CC_TYPE_SETTING_EDITOR, NULL);
  seditor->priv->s_type = s_type;
  seditor->priv->key = g_strdup (key);
  seditor->priv->ui_control = ui_control;

  if (settings_prefix != NULL) {
    seditor->priv->settings = g_settings_new (settings_prefix);
    seditor->priv->settings_prefix = g_strdup (settings_prefix);
  }

  /* Add the ui control to the box */
  gtk_box_pack_start (GTK_BOX (seditor), ui_control, TRUE, FALSE, 3);

  return seditor;
}

static void
application_selection_changed_cb (GtkComboBox *combobox, CcSettingEditor *seditor)
{
  GtkTreeIter selected_row;

  if (gtk_combo_box_get_active_iter (combobox, &selected_row)) {
    GAppInfo *app_info;
    GError *error = NULL;

    gtk_tree_model_get (gtk_combo_box_get_model (combobox), &selected_row,
                         APP_INFO_COL, &app_info,
                         -1);
    if (app_info != NULL) {
      if (!g_app_info_set_as_default_for_type (app_info, seditor->priv->key, &error)) {
        g_warning ("Could not set %s as default app for %s: %s",
                   g_app_info_get_display_name (app_info),
                   seditor->priv->key,
                   error->message);
      }
    }
  }
}

static gboolean
is_separator (GtkTreeModel *model, GtkTreeIter *iter, gpointer sep_index)
{
    GtkTreePath *path;
    gboolean result;

    path = gtk_tree_model_get_path (model, iter);
    result = gtk_tree_path_get_indices (path)[0] == GPOINTER_TO_INT (sep_index);
    gtk_tree_path_free (path);

    return result;
}

/**
 * cc_setting_editor_new_application:
 * @mime_type: The MIME type to configure application for.
 *
 * Create a new #CCSettingEditor object to configure the default application
 * for the given MIME type.
 *
 * Return value: A newly created #CCSettingEditor object.
 */
GtkWidget *
cc_setting_editor_new_application (const gchar *mime_type)
{
  CcSettingEditor *seditor;
  GList *app_list;
  GtkListStore *model;
  GtkCellRenderer *renderer;
  GAppInfo *default_app;
  GtkWidget *combobox;

  /* Setup the combo box */
  model = gtk_list_store_new (3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER);
  combobox = gtk_combo_box_new_with_model (GTK_TREE_MODEL (model));
  gtk_widget_show (combobox);

  /* Create the CcSettingEditor widget */
  seditor = cc_setting_editor_new (SETTING_TYPE_APPLICATION,
                                   NULL,
                                   mime_type,
                                   combobox);

  renderer = gtk_cell_renderer_pixbuf_new ();
  /* not all cells have a pixbuf, this prevents the combo box to shrink */
  gtk_cell_renderer_set_fixed_size (renderer, -1, 22);
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                  "pixbuf", PIXBUF_COL,
                                  NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer,
                                  "text", TEXT_COL,
                                  NULL);

  gtk_combo_box_set_model (GTK_COMBO_BOX (combobox), GTK_TREE_MODEL (model));

  /* Retrieve list of applications for the given MIME type */
  default_app = g_app_info_get_default_for_type (mime_type, FALSE);

  app_list = g_app_info_get_all_for_type (mime_type);
  gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (combobox), is_separator,
                                        GINT_TO_POINTER (g_list_length (app_list)), NULL);

  while (app_list != NULL) {
    GtkTreeIter new_row;
    GAppInfo *app_info = (GAppInfo *) app_list->data;

    g_debug ("Adding application %s", g_app_info_get_display_name (app_info));

    gtk_list_store_append (model, &new_row);
    gtk_list_store_set (model, &new_row,
                        PIXBUF_COL, /* FIXME */ NULL,
                        TEXT_COL, g_app_info_get_name (app_info),
                        APP_INFO_COL, app_info,
                        -1);

    /* If it's the default one, select it */
    if (g_str_equal (g_app_info_get_name (default_app),
                     g_app_info_get_name (app_info))) {
      gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &new_row);
    }

    app_list = g_list_remove (app_list, app_info);
  }

  g_signal_connect (combobox, "changed",
                    G_CALLBACK (application_selection_changed_cb), seditor);

  return (GtkWidget *) seditor;
}

static void
boolean_value_changed_cb (GtkToggleButton *toggle_button, CcSettingEditor *seditor)
{
  g_settings_set_boolean (seditor->priv->settings,
                          seditor->priv->key,
                          gtk_toggle_button_get_active (toggle_button));
}

/**
 * cc_setting_editor_new_boolean:
 * @settings_prefix: The settings prefix for the key.
 * @key: The settings key to associate with.
 *
 * Create a new #CCSettingEditor object to configure a boolean setting.
 *
 * Return value: A newly created #CCSettingEditor object.
 */
GtkWidget *
cc_setting_editor_new_boolean (const gchar *label,
                               const gchar *settings_prefix,
                               const gchar *key)
{
  GtkWidget *checkbox;
  CcSettingEditor *seditor;

  checkbox = gtk_check_button_new_with_label (label);
  gtk_widget_show (checkbox);

  /* Create the CcSettingEditor widget */
  seditor = cc_setting_editor_new (SETTING_TYPE_GSETTINGS,
                                   settings_prefix,
                                   key,
                                   checkbox);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbox),
                                g_settings_get_boolean (seditor->priv->settings, seditor->priv->key));
  g_signal_connect (checkbox, "toggled",
                    G_CALLBACK (boolean_value_changed_cb), seditor);

  return (GtkWidget *) seditor;
}

/**
 * cc_setting_editor_get_key:
 * @seditor: a #CCSettingEditor object.
 *
 * Get the settings key associated with the given #CCSettingEditor object.
 *
 * Return value: The settings key associated with the given #CCSettingEditor object.
 */
const gchar *
cc_setting_editor_get_key (CcSettingEditor *seditor)
{
  g_return_val_if_fail (CC_IS_SETTING_EDITOR (seditor), NULL);

  return seditor->priv->key;
}

/**
 * cc_setting_editor_get_ui_control:
 * @seditor: a #CCSettingEditor object.
 *
 * Retrieve the UI control associated with the given #CCSettingEditor object.
 *
 * Return value: The UI control associated with the given #CCSettingEditor.
 */
GtkWidget *
cc_setting_editor_get_ui_control (CcSettingEditor *seditor)
{
  g_return_val_if_fail (CC_IS_SETTING_EDITOR (seditor), NULL);

  return seditor->priv->ui_control;
}
