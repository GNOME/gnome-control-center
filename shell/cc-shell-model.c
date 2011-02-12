/*
 * Copyright (c) 2009, 2010 Intel, Inc.
 * Copyright (c) 2010 Red Hat, Inc.
 *
 * The Control Center is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * The Control Center is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with the Control Center; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Thomas Wood <thos@gnome.org>
 */

#include "cc-shell-model.h"
#include <string.h>

#define GNOME_SETTINGS_PANEL_ID_KEY "X-GNOME-Settings-Panel"
#define GNOME_SETTINGS_PANEL_CATEGORY GNOME_SETTINGS_PANEL_ID_KEY
#define GNOME_SETTINGS_PANEL_ID_KEYWORDS "X-GNOME-Keywords"


G_DEFINE_TYPE (CcShellModel, cc_shell_model, GTK_TYPE_LIST_STORE)

static GdkPixbuf *
load_pixbuf_for_string (const char *icon)
{
  GtkIconTheme *theme;
  GdkPixbuf *pixbuf;
  GError *err = NULL;
  char *icon2 = NULL;

  if (icon == NULL)
    return NULL;

  theme = gtk_icon_theme_get_default ();

  /* find the icon */
  if (*icon == '/')
    {
      pixbuf = gdk_pixbuf_new_from_file_at_scale (icon, 32, 32, TRUE, &err);
    }
  else
    {
      if (g_str_has_suffix (icon, ".png"))
        icon2 = g_strndup (icon, strlen (icon) - strlen (".png"));

      pixbuf = gtk_icon_theme_load_icon (theme,
                                         icon2 ? icon2 : icon, 32,
                                         GTK_ICON_LOOKUP_FORCE_SIZE,
                                         &err);
    }

  if (err)
    {
      g_warning ("Could not load icon '%s': %s", icon2 ? icon2 : icon,
                 err->message);
      g_error_free (err);
    }

  g_free (icon2);

  return pixbuf;
}

static void
icon_theme_changed (GtkIconTheme *theme,
                    CcShellModel *self)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  gboolean cont;

  model = GTK_TREE_MODEL (self);
  cont = gtk_tree_model_get_iter_first (model, &iter);
  while (cont)
    {
      GdkPixbuf *pixbuf;
      char *icon;

      gtk_tree_model_get (model, &iter,
                          COL_ICON_NAME, &icon,
                          -1);
      pixbuf = load_pixbuf_for_string (icon);
      g_free (icon);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          COL_PIXBUF, pixbuf,
                          -1);

      cont = gtk_tree_model_iter_next (model, &iter);
    }
}

static void
cc_shell_model_class_init (CcShellModelClass *klass)
{
}

static void
cc_shell_model_init (CcShellModel *self)
{
  GType types[] = {G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
      GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRV};

  gtk_list_store_set_column_types (GTK_LIST_STORE (self),
                                   N_COLS, types);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self), COL_NAME,
                                        GTK_SORT_ASCENDING);

  g_signal_connect (G_OBJECT (gtk_icon_theme_get_default ()), "changed",
                    G_CALLBACK (icon_theme_changed), self);
}

CcShellModel *
cc_shell_model_new (void)
{
  return g_object_new (CC_TYPE_SHELL_MODEL, NULL);
}

static gboolean
desktop_entry_has_panel_category (GKeyFile *key_file)
{
  char   **strv;
  gsize    len;
  int      i;

  strv = g_key_file_get_string_list (key_file,
				     "Desktop Entry",
				     "Categories",
				     &len,
				     NULL);
  if (!strv)
    return FALSE;

  for (i = 0; strv[i]; i++)
    {
      if (g_str_equal (strv[i], GNOME_SETTINGS_PANEL_CATEGORY))
        {
          g_strfreev (strv);
          return TRUE;
	}
    }

  g_strfreev (strv);

  return FALSE;

}

void
cc_shell_model_add_item (CcShellModel   *model,
                         const gchar    *category_name,
                         GMenuTreeEntry *item)
{
  const gchar *icon = gmenu_tree_entry_get_icon (item);
  const gchar *name = gmenu_tree_entry_get_name (item);
  const gchar *desktop = gmenu_tree_entry_get_desktop_file_path (item);
  const gchar *comment = gmenu_tree_entry_get_comment (item);
  gchar *id;
  GdkPixbuf *pixbuf = NULL;
  gchar *search_target;
  GKeyFile *key_file;
  gchar **keywords;

  /* load the .desktop file since gnome-menus doesn't have a way to read
   * custom properties from desktop files */

  key_file = g_key_file_new ();
  g_key_file_load_from_file (key_file, desktop, 0, NULL);

  id = g_key_file_get_string (key_file, "Desktop Entry",
                              GNOME_SETTINGS_PANEL_ID_KEY, NULL);

  if (!id)
    {
      /* Refuse to load desktop files without a panel ID, but
       * with the X-GNOME-Settings-Panel category */
      if (desktop_entry_has_panel_category (key_file))
        {
          g_warning ("Not loading desktop file '%s' because it uses the "
		     GNOME_SETTINGS_PANEL_CATEGORY
		     " category but isn't a panel.",
		     desktop);
         g_key_file_free (key_file);
         return;
	}
      id = g_strdup (gmenu_tree_entry_get_desktop_file_id (item));
    }

  keywords = g_key_file_get_locale_string_list (key_file, "Desktop Entry",
                                                GNOME_SETTINGS_PANEL_ID_KEYWORDS,
                                                NULL, NULL, NULL);

  g_key_file_free (key_file);
  key_file = NULL;

  pixbuf = load_pixbuf_for_string (icon);

  search_target = g_strconcat (name, " - ", comment, NULL);

  gtk_list_store_insert_with_values (GTK_LIST_STORE (model), NULL, 0,
                                     COL_NAME, name,
                                     COL_DESKTOP_FILE, desktop,
                                     COL_ID, id,
                                     COL_PIXBUF, pixbuf,
                                     COL_CATEGORY, category_name,
                                     COL_SEARCH_TARGET, search_target,
                                     COL_ICON_NAME, icon,
                                     COL_KEYWORDS, keywords,
                                     -1);

  g_free (id);
  g_free (search_target);
  g_strfreev (keywords);
}
