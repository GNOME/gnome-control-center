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
#include "appearance.h"

#include <string.h>

#include "gnome-theme-info.h"
#include "gconf-property-editor.h"

enum ThemeType {
  GTK_THEMES,
  METACITY_THEMES,
  ICON_THEMES,
  CURSOR_THEMES,
  COLOR_SCHEME
};

static const gchar *gconf_keys[] = {
  "/desktop/gnome/interface/gtk_theme",
  "/apps/metacity/general/theme",
  "/desktop/gnome/interface/icon_theme",
  "/desktop/gnome/peripherals/mouse/cursor_theme",
  "/desktop/gnome/interface/gtk_color_scheme"
};


static void prepare_combo (AppearanceData *data, GtkWidget *combo, enum ThemeType type);
static void update_color_buttons_from_string (gchar *color_scheme, AppearanceData *data);

static void color_scheme_changed (GObject    *settings, GParamSpec *pspec, AppearanceData  *data);


/* GUI Callbacks */

static void color_button_clicked_cb (GtkWidget *colorbutton, AppearanceData *data);
static GConfValue *conv_to_widget_cb (GConfPropertyEditor *peditor, GConfValue *value);
static GConfValue *conv_from_widget_cb (GConfPropertyEditor *peditor, GConfValue *value);


void
style_init (AppearanceData *data)
{

  GtkSettings *settings;
  gchar *colour_scheme;
  GtkWidget *w;

  w = glade_xml_get_widget (data->xml, "theme_details");
  g_signal_connect_swapped (glade_xml_get_widget (data->xml, "theme_close_button"), "clicked", (GCallback) gtk_widget_hide, w);
  gtk_widget_hide_on_delete (w);

  prepare_combo (data, glade_xml_get_widget (data->xml, "gtk_themes_combobox"), GTK_THEMES);
  prepare_combo (data, glade_xml_get_widget (data->xml, "window_themes_combobox"), METACITY_THEMES);
  prepare_combo (data, glade_xml_get_widget (data->xml, "icon_themes_combobox"), ICON_THEMES);

  settings = gtk_settings_get_default ();
  g_object_get (G_OBJECT (settings), "gtk-color-scheme", &colour_scheme, NULL);
  g_signal_connect (G_OBJECT (settings), "notify::gtk-color-scheme", (GCallback) color_scheme_changed, data);
  update_color_buttons_from_string (colour_scheme, data);
  g_free (colour_scheme);

  /* connect signals */
  /* color buttons */
  g_signal_connect (G_OBJECT (glade_xml_get_widget (data->xml, "bg_colorbutton")), "color-set", (GCallback) color_button_clicked_cb, data);
  g_signal_connect (G_OBJECT (glade_xml_get_widget (data->xml, "base_colorbutton")), "color-set", (GCallback) color_button_clicked_cb, data);
  g_signal_connect (G_OBJECT (glade_xml_get_widget (data->xml, "selected_bg_colorbutton")), "color-set", (GCallback) color_button_clicked_cb, data);


}

static void
prepare_combo (AppearanceData *data, GtkWidget *combo, enum ThemeType type)
{
  GtkListStore *store;
  GList *l, *list = NULL;
  GtkCellRenderer *renderer;
  GnomeThemeElement element = 0;
  GtkTreeModel *sort_model;

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

    default:
      /* we don't deal with any other type of themes here */
      return;
  }
  if (!list)
    return;

  renderer = gtk_cell_renderer_text_new ();
  store = gtk_list_store_new (1, G_TYPE_STRING);

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

  }

  sort_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model), 0, GTK_SORT_ASCENDING);

  gtk_combo_box_set_model (GTK_COMBO_BOX (combo), GTK_TREE_MODEL (sort_model));
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), renderer, TRUE);
  gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), renderer, "text", 0);

  gconf_peditor_new_combo_box (NULL, gconf_keys[type], combo,
    "conv-to-widget-cb", conv_to_widget_cb,
    "conv-from-widget-cb", conv_from_widget_cb,
    NULL);


}

/* Callbacks */

static int
find_string_in_model (GtkTreeModel *model, const gchar *value, gint column)
{
  gint index = -1;
  GtkTreeIter iter;
  gboolean found, valid;
  gchar *test = NULL;

  if (!value)
    return -1;

  valid = gtk_tree_model_get_iter_first (model, &iter);
  found = FALSE;
  while (valid)
  {
    index++;
    g_free (test);
    gtk_tree_model_get (model, &iter, column, &test, -1);
    if (test && !strcmp (test, value))
    {
      found = TRUE;
      break;
    }
    valid = gtk_tree_model_iter_next (model, &iter);
  }
  g_free (test);

  return (found) ? index : -1;

}

static GConfValue *
conv_to_widget_cb (GConfPropertyEditor *peditor, GConfValue *value)
{
  GtkTreeModel *store;
  GtkWidget *combo;
  const gchar *curr_value;
  GConfValue *new_value;
  gint index  = -1;

  /* find value in model */
  curr_value = gconf_value_get_string (value);
  combo = GTK_WIDGET (gconf_property_editor_get_ui_control (peditor));
  store = gtk_combo_box_get_model (GTK_COMBO_BOX (combo));

  index = find_string_in_model (store, curr_value, 0);


  /* Add a temporary item if we can't find a match
   * TODO: delete this item if it is no longer selected?
   */
  if (index == -1)
  {
    GtkListStore *list_store;

    if (GTK_IS_TREE_MODEL_SORT (store))
      list_store = GTK_LIST_STORE (gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (store)));
    else
      list_store = GTK_LIST_STORE (store);

    gtk_list_store_insert_with_values (list_store, NULL, 0, 0, curr_value, -1);
    /* if the model is sorted then it might not still be in the position we just
     * placed it, so we have to search the model again... */
    index = find_string_in_model (store, curr_value, 0);
  }


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
update_color_buttons_from_string (gchar *color_scheme, AppearanceData *data)
{
  GdkColor color_scheme_colors[6];
  gchar **color_scheme_strings, **color_scheme_pair, *current_string;
  gint i;
  GtkWidget *widget;

  if (!color_scheme) return;
  if (!strcmp (color_scheme, "")) return;

  /* The color scheme string consists of name:color pairs, seperated by
   * newlines, so first we split the string up by new line */

  color_scheme_strings = g_strsplit (color_scheme, "\n", 0);

  /* loop through the name:color pairs, and save the colour if we recognise the name */
  i = 0;
  while ((current_string = color_scheme_strings[i++]))
  {
    color_scheme_pair = g_strsplit (current_string, ":", 0);

    if (color_scheme_pair[0] != NULL && color_scheme_pair[1] != NULL)
    {
      g_strstrip (color_scheme_pair[0]);
      g_strstrip (color_scheme_pair[1]);

      if (!strcmp ("fg_color", color_scheme_pair[0]))
        gdk_color_parse (color_scheme_pair[1], &color_scheme_colors[0]);
      else if (!strcmp ("bg_color", color_scheme_pair[0]))
        gdk_color_parse (color_scheme_pair[1], &color_scheme_colors[1]);
      else if (!strcmp ("text_color", color_scheme_pair[0]))
        gdk_color_parse (color_scheme_pair[1], &color_scheme_colors[2]);
      else if (!strcmp ("base_color", color_scheme_pair[0]))
        gdk_color_parse (color_scheme_pair[1], &color_scheme_colors[3]);
      else if (!strcmp ("selected_fg_color", color_scheme_pair[0]))
        gdk_color_parse (color_scheme_pair[1], &color_scheme_colors[4]);
      else if (!strcmp ("selected_bg_color", color_scheme_pair[0]))
        gdk_color_parse (color_scheme_pair[1], &color_scheme_colors[5]);
    }

    g_strfreev (color_scheme_pair);
  }

  g_strfreev (color_scheme_strings);

  /* not sure whether we need to do this, but it can't hurt */
  for (i = 0; i < 6; i++)
    gdk_colormap_alloc_color (gdk_colormap_get_system (), &color_scheme_colors[i], FALSE, TRUE);

  /* now set all the buttons to the correct settings */
  widget = glade_xml_get_widget (data->xml, "bg_colorbutton");
  gtk_color_button_set_color (GTK_COLOR_BUTTON (widget), &color_scheme_colors[1]);
  widget = glade_xml_get_widget (data->xml, "base_colorbutton");
  gtk_color_button_set_color (GTK_COLOR_BUTTON (widget), &color_scheme_colors[3]);
  widget = glade_xml_get_widget (data->xml, "selected_bg_colorbutton");
  gtk_color_button_set_color (GTK_COLOR_BUTTON (widget), &color_scheme_colors[5]);


}

static void
color_scheme_changed (GObject    *settings,
                      GParamSpec *pspec,
                      AppearanceData  *data)
{
  gchar *theme;

  theme = gconf_client_get_string (data->client, gconf_keys[COLOR_SCHEME], NULL);
  if (theme == NULL || strcmp (theme, "") == 0)
    g_object_get (settings, "gtk-color-scheme", &theme, NULL);

  update_color_buttons_from_string (theme, data);
  g_free (theme);
}

static void
color_button_clicked_cb (GtkWidget *colorbutton, AppearanceData *data)
{
  gchar *new_scheme;
  GdkColor colors[6];
  gchar *bg, *fg, *text, *base, *selected_fg, *selected_bg;
  GtkWidget *widget;

  widget = glade_xml_get_widget (data->xml, "bg_colorbutton");
  gtk_color_button_get_color (GTK_COLOR_BUTTON (widget), &colors[1]);
  widget = glade_xml_get_widget (data->xml, "base_colorbutton");
  gtk_color_button_get_color (GTK_COLOR_BUTTON (widget), &colors[3]);
  widget = glade_xml_get_widget (data->xml, "selected_bg_colorbutton");
  gtk_color_button_get_color (GTK_COLOR_BUTTON (widget), &colors[5]);

  /* TODO: calculate proper colours here */
  gdk_color_parse ("black", &colors[0]);
  gdk_color_parse ("black", &colors[2]);
  gdk_color_parse ("black", &colors[4]);

  fg = g_strdup_printf ("fg_color:#%04x%04x%04x\n", colors[0].red, colors[0].green, colors[0].blue);
  bg = g_strdup_printf ("bg_color:#%04x%04x%04x\n", colors[1].red, colors[1].green, colors[1].blue);
  text = g_strdup_printf ("text_color:#%04x%04x%04x\n", colors[2].red, colors[2].green, colors[2].blue);
  base = g_strdup_printf ("base_color:#%04x%04x%04x\n", colors[3].red, colors[3].green, colors[3].blue);
  selected_fg = g_strdup_printf ("selected_fg_color:#%04x%04x%04x\n", colors[4].red, colors[4].green, colors[4].blue);
  selected_bg = g_strdup_printf ("selected_bg_color:#%04x%04x%04x", colors[5].red, colors[5].green, colors[5].blue);

  new_scheme = g_strconcat (fg, bg, text, base, selected_fg, selected_bg, NULL);

  /* Currently we assume this has only been called when one of the colours has
   * actually changed, so we don't check the original key first
   */
  gconf_client_set_string (data->client, gconf_keys[COLOR_SCHEME], new_scheme, NULL);

  g_free (fg);
  g_free (bg);
  g_free (text);
  g_free (base);
  g_free (selected_fg);
  g_free (selected_bg);
  g_free (new_scheme);
}


