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
#include <pango/pango.h>

#include "gnome-theme-info.h"
#include "gconf-property-editor.h"
#include "theme-thumbnail.h"

enum ThemeType {
  GTK_THEMES,
  METACITY_THEMES,
  ICON_THEMES,
  CURSOR_THEMES,
  COLOR_SCHEME
};

enum {
  COL_THUMBNAIL,
  COL_LABEL,
  COL_NAME,
  NUM_COLS
};

static const gchar *gconf_keys[] = {
  "/desktop/gnome/interface/gtk_theme",
  "/apps/metacity/general/theme",
  "/desktop/gnome/interface/icon_theme",
  "/desktop/gnome/peripherals/mouse/cursor_theme",
  "/desktop/gnome/interface/gtk_color_scheme"
};


static void prepare_list (AppearanceData *data, GtkWidget *list, enum ThemeType type);
static void update_color_buttons_from_string (const gchar *color_scheme, AppearanceData *data);

static void color_scheme_changed (GObject *settings, GParamSpec *pspec, AppearanceData *data);


/* GUI Callbacks */

static void color_button_clicked_cb (GtkWidget *colorbutton, AppearanceData *data);
static GConfValue *conv_to_widget_cb (GConfPropertyEditor *peditor, const GConfValue *value);
static GConfValue *conv_from_widget_cb (GConfPropertyEditor *peditor, const GConfValue *value);


void
style_init (AppearanceData *data)
{

  GObject *settings;
  gchar *colour_scheme;
  GtkWidget *w;

  w = glade_xml_get_widget (data->xml, "theme_details");
  g_signal_connect_swapped (glade_xml_get_widget (data->xml, "theme_close_button"), "clicked", (GCallback) gtk_widget_hide, w);
  gtk_widget_hide_on_delete (w);

  prepare_list (data, glade_xml_get_widget (data->xml, "gtk_themes_list"), GTK_THEMES);
  prepare_list (data, glade_xml_get_widget (data->xml, "window_themes_list"), METACITY_THEMES);
  prepare_list (data, glade_xml_get_widget (data->xml, "icon_themes_list"), ICON_THEMES);

  settings = G_OBJECT (gtk_settings_get_default ());
  g_object_get (settings, "gtk-color-scheme", &colour_scheme, NULL);
  g_signal_connect (settings, "notify::gtk-color-scheme", (GCallback) color_scheme_changed, data);
  update_color_buttons_from_string (colour_scheme, data);
  g_free (colour_scheme);

  /* connect signals */
  /* color buttons */
  g_signal_connect (G_OBJECT (glade_xml_get_widget (data->xml, "bg_colorbutton")), "color-set", (GCallback) color_button_clicked_cb, data);
  g_signal_connect (G_OBJECT (glade_xml_get_widget (data->xml, "base_colorbutton")), "color-set", (GCallback) color_button_clicked_cb, data);
  g_signal_connect (G_OBJECT (glade_xml_get_widget (data->xml, "selected_bg_colorbutton")), "color-set", (GCallback) color_button_clicked_cb, data);
}

static void
prepare_list (AppearanceData *data, GtkWidget *list, enum ThemeType type)
{
  GtkListStore *store;
  GList *l, *themes = NULL;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkTreeModel *sort_model;

  switch (type)
  {
    case GTK_THEMES:
      themes = gnome_theme_info_find_by_type (GNOME_THEME_GTK_2);
      break;

    case METACITY_THEMES:
      themes = gnome_theme_info_find_by_type (GNOME_THEME_METACITY);
      break;

    case ICON_THEMES:
      themes = gnome_theme_icon_info_find_all ();
      break;

    case CURSOR_THEMES:
      themes = NULL; /* don't know what to do yet */

    default:
      /* we don't deal with any other type of themes here */
      return;
  }
  if (!list)
    return;

  store = gtk_list_store_new (NUM_COLS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);

  for (l = themes; l; l = g_list_next (l))
  {
    const gchar *name = NULL;
    const gchar *label = NULL;
    GdkPixbuf *thumbnail;
    GtkTreeIter i;

    if (type == GTK_THEMES || type == METACITY_THEMES) {
      name = ((GnomeThemeInfo *) l->data)->name;
    } else if (type == ICON_THEMES) {
      name = ((GnomeThemeIconInfo *) l->data)->name;
      label = ((GnomeThemeIconInfo *) l->data)->readable_name;
    }

    if (!name)
      continue; /* just in case... */

    switch (type)
    {
      case GTK_THEMES:
        thumbnail = generate_gtk_theme_thumbnail ((GnomeThemeInfo *) l->data);
        break;

      case ICON_THEMES:
        thumbnail = generate_icon_theme_thumbnail ((GnomeThemeIconInfo *) l->data);
        break;
      
      case METACITY_THEMES:
        thumbnail = generate_metacity_theme_thumbnail ((GnomeThemeInfo *) l->data);
        break;

      default:
        thumbnail = NULL;
    } 

    gtk_list_store_insert_with_values (store, &i, 0,
                                       COL_THUMBNAIL, thumbnail,
                                       COL_LABEL, label ? label : name,
                                       COL_NAME, name, -1);
    if (thumbnail)
      g_object_unref (thumbnail);
  }

  sort_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
                                        COL_LABEL, GTK_SORT_ASCENDING);

  gtk_tree_view_set_model (GTK_TREE_VIEW (list), GTK_TREE_MODEL (sort_model));

  renderer = gtk_cell_renderer_pixbuf_new ();
  g_object_set (G_OBJECT (renderer),
    "xpad", 3,
    "ypad", 3,
    NULL);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);  
  gtk_tree_view_column_add_attribute (column, renderer, "pixbuf", COL_THUMBNAIL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer),
    "weight", PANGO_WEIGHT_BOLD,
    "weight-set", TRUE,
    NULL);

  column = gtk_tree_view_column_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);  
  gtk_tree_view_column_add_attribute (column, renderer, "text", COL_LABEL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (list), column);

  gconf_peditor_new_tree_view (NULL, gconf_keys[type], list,
    "conv-to-widget-cb", conv_to_widget_cb,
    "conv-from-widget-cb", conv_from_widget_cb,
    NULL);
}

/* Callbacks */

static gchar *
find_string_in_model (GtkTreeModel *model, const gchar *value, gint column)
{
  GtkTreeIter iter;
  gboolean valid;
  gchar *path = NULL, *test;

  if (!value)
    return NULL;

  for (valid = gtk_tree_model_get_iter_first (model, &iter); valid;
       valid = gtk_tree_model_iter_next (model, &iter))
  {
    gtk_tree_model_get (model, &iter, column, &test, -1);

    if (test)
    {
      gint cmp = strcmp (test, value);
      g_free (test);

      if (!cmp)
      {
        path = gtk_tree_model_get_string_from_iter (model, &iter);
        break;
      }
    }
  }

  return path;
}

static GConfValue *
conv_to_widget_cb (GConfPropertyEditor *peditor, const GConfValue *value)
{
  GtkTreeModel *store;
  GtkTreeView *list;
  const gchar *curr_value;
  GConfValue *new_value;
  gchar *path;

  /* find value in model */
  curr_value = gconf_value_get_string (value);
  list = GTK_TREE_VIEW (gconf_property_editor_get_ui_control (peditor));
  store = gtk_tree_view_get_model (list);

  path = find_string_in_model (store, curr_value, COL_NAME);

  /* Add a temporary item if we can't find a match
   * TODO: delete this item if it is no longer selected?
   */
  if (!path)
  {
    GtkListStore *list_store;
    GtkTreeIter iter, sort_iter;

    list_store = GTK_LIST_STORE (gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (store)));

    gtk_list_store_insert_with_values (list_store, &iter, 0,
                                       COL_LABEL, curr_value,
                                       COL_NAME, curr_value, -1);
    /* convert the tree store iter for use with the sort model */
    gtk_tree_model_sort_convert_child_iter_to_iter (GTK_TREE_MODEL_SORT (store),
                                                    &sort_iter, &iter);
    path = gtk_tree_model_get_string_from_iter (store, &sort_iter);
  }

  new_value = gconf_value_new (GCONF_VALUE_STRING);
  gconf_value_set_string (new_value, path);
  g_free (path);

  return new_value;
}

static GConfValue *
conv_from_widget_cb (GConfPropertyEditor *peditor, const GConfValue *value)
{
  GConfValue *new_value;
  gchar *list_value = NULL;
  GtkTreeIter iter;
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeView *list;

  list = GTK_TREE_VIEW (gconf_property_editor_get_ui_control (peditor));
  model = gtk_tree_view_get_model (list);

  selection = gtk_tree_view_get_selection (list);
  gtk_tree_selection_get_selected (selection, NULL, &iter);
  gtk_tree_model_get (model, &iter, COL_NAME, &list_value, -1);

  new_value = gconf_value_new (GCONF_VALUE_STRING);
  gconf_value_set_string (new_value, list_value);
  g_free (list_value);

  return new_value;
}

static void
update_color_buttons_from_string (const gchar *color_scheme, AppearanceData *data)
{
  GdkColor color_scheme_colors[6];
  gchar **color_scheme_strings, **color_scheme_pair, *current_string;
  gint i;
  GtkWidget *widget;

  if (!color_scheme || !strcmp (color_scheme, "")) return;

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
