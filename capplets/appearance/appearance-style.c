/*
 * Copyright (C) 2007 The GNOME Foundation
 * Written by Thomas Wood <thos@gnome.org>
 * All Rights Reserved
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <gtk/gtk.h>
#include <string.h>

#include "gnome-theme-info.h"
#include "appearance.h"
#include "gconf-property-editor.h"

enum ThemeType {
  GTK_THEMES,
  METACITY_THEMES,
  ICON_THEMES,
  CURSOR_THEMES
};

static gchar *gconf_keys[] = {
  "/desktop/gnome/interface/gtk_theme",
  "/apps/metacity/general/theme",
  "/desktop/gnome/interface/icon_theme",
  "/desktop/gnome/peripherals/mouse/cursor_theme"
};

static GConfValue *
conv_to_widget_cb (GConfPropertyEditor *peditor, GConfValue *value)
{
  GtkListStore *store;
  GtkTreeIter iter;
  gboolean valid;
  gchar *test = NULL;
  GtkWidget *combo;
  GConfValue *new_value;
  gint index  = -1;

  /* find value in model */
  combo = GTK_WIDGET (gconf_property_editor_get_ui_control (peditor));
  store = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (combo)));
  valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter);
  while (valid)
  {
    index++;
    g_free (test);
    gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 0, &test, -1);
    if (test && !strcmp (test, gconf_value_get_string (value)))
      break;
    valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter);
  }
  g_free (test);

  new_value = gconf_value_new (GCONF_VALUE_INT);
  gconf_value_set_int (new_value, index);

  return new_value;
}

static GConfValue *
conv_from_widget_cb (GConfPropertyEditor *peditor, GConfValue *value)
{
  GConfValue *new_value;
  gchar *combo_value = NULL;
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkWidget *combo;

  combo = GTK_WIDGET (gconf_property_editor_get_ui_control (peditor));
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));
  gtk_combo_box_get_active_iter (GTK_COMBO_BOX (combo), &iter);
  gtk_tree_model_get (model, &iter, 0, &combo_value, -1);

  new_value = gconf_value_new (GCONF_VALUE_STRING);
  gconf_value_set_string (new_value, combo_value);
  g_free (combo_value);

  return new_value;
}

static void
prepare_combo (AppearanceData *data, GtkWidget *combo, enum ThemeType type)
{
  GtkListStore *store;
  GList *l, *list = NULL;
  GtkCellRenderer *renderer;
  GnomeThemeElement element = 0;
  gchar *value = NULL;
  GtkTreeIter active_row;

  switch (type)
  {
    case GTK_THEMES:
      element = GNOME_THEME_GTK_2;
    case METACITY_THEMES:
      if (!element) element = GNOME_THEME_METACITY;
      list = gnome_theme_info_find_by_type (element);
      break;

    case ICON_THEMES:
      list = gnome_theme_icon_info_find_all ();
      break;

    case CURSOR_THEMES:
      list = NULL; /* don't know what to do yet */
  }
  if (!list)
    return;

  renderer = gtk_cell_renderer_text_new ();
  store = gtk_list_store_new (1, G_TYPE_STRING);
  value = gconf_client_get_string (data->client, gconf_keys[type], NULL);

  for (l = list; l; l = g_list_next (l))
  {
    gchar *name = NULL;
    GtkTreeIter i;

    if (type < ICON_THEMES)
      name = ((GnomeThemeInfo*) l->data)->name;
    else if (type == ICON_THEMES)
      name = ((GnomeThemeIconInfo*) l->data)->name;

    if (!name)
      continue; /* just in case... */

    gtk_list_store_insert_with_values (store, &i, 0, 0, name, -1);

    if (value && !strcmp (value, name))
      active_row = i;
  }

  gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (store));
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), renderer, "text", 0);
  gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo), &active_row);

  gconf_peditor_new_combo_box (NULL, gconf_keys[type], combo,
    "conv-to-widget-cb", conv_to_widget_cb,
    "conv-from-widget-cb", conv_from_widget_cb,
    NULL);


}
void
style_init (AppearanceData *data)
{

  prepare_combo (data, glade_xml_get_widget (data->xml, "gtk_themes_combobox"), GTK_THEMES);
  prepare_combo (data, glade_xml_get_widget (data->xml, "window_themes_combobox"), METACITY_THEMES);
  prepare_combo (data, glade_xml_get_widget (data->xml, "icon_themes_combobox"), ICON_THEMES);

}
