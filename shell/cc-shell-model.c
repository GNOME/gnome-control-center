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


G_DEFINE_TYPE (CcShellModel, cc_shell_model, GTK_TYPE_LIST_STORE)

static void
cc_shell_model_class_init (CcShellModelClass *klass)
{

}

static void
cc_shell_model_init (CcShellModel *self)
{
  GType types[] = {G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
      GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING};

  gtk_list_store_set_column_types (GTK_LIST_STORE (self),
                                   N_COLS, types);
}

CcShellModel *
cc_shell_model_new (void)
{
  return g_object_new (CC_TYPE_SHELL_MODEL, NULL);
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
  gchar *icon2 = NULL;
  GError *err = NULL;
  gchar *search_target;
  GKeyFile *key_file;

  /* load the .desktop file since gnome-menus doesn't have a way to read
   * custom properties from desktop files */

  key_file = g_key_file_new ();
  g_key_file_load_from_file (key_file, desktop, 0, NULL);

  id = g_key_file_get_string (key_file, "Desktop Entry",
                              GNOME_SETTINGS_PANEL_ID_KEY, NULL);
  g_key_file_free (key_file);
  key_file = NULL;

  if (!id)
    id = g_strdup (gmenu_tree_entry_get_desktop_file_id (item));

  /* find the icon */
  if (icon != NULL && *icon == '/')
    {
      pixbuf = gdk_pixbuf_new_from_file_at_scale (icon, 32, 32, TRUE, &err);
    }
  else
    {
      if (icon2 == NULL && icon != NULL && g_str_has_suffix (icon, ".png"))
        icon2 = g_strndup (icon, strlen (icon) - strlen (".png"));

      pixbuf = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (),
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

  search_target = g_strconcat (name, " - ", comment, NULL);

  gtk_list_store_insert_with_values (GTK_LIST_STORE (model), NULL, 0,
                                     COL_NAME, name,
                                     COL_DESKTOP_FILE, desktop,
                                     COL_ID, id,
                                     COL_PIXBUF, pixbuf,
                                     COL_CATEGORY, category_name,
                                     COL_SEARCH_TARGET, search_target,
                                     COL_ICON_NAME, icon,
                                     -1);

  g_free (id);
  g_free (search_target);
}
