/*
 * Copyright (C) 2007 The GNOME Foundation
 * Written by Thomas Wood <thos@gnome.org>
 *            Jens Granseuer <jensgr@gmx.net>
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

#include <glib/gi18n.h>

#include "theme-util.h"

gboolean
theme_delete (const gchar *name, ThemeType type)
{
  GtkWidget *confirm_dialog;
  gpointer theme;
  gchar *theme_dir;
  gint response;
  GList *uri_list;
  GnomeVFSResult result;

  confirm_dialog = gtk_message_dialog_new (NULL,
		          		   GTK_DIALOG_MODAL,
					   GTK_MESSAGE_QUESTION,
					   GTK_BUTTONS_OK_CANCEL,
					   _("Would you like to remove this theme?"));
  response = gtk_dialog_run (GTK_DIALOG (confirm_dialog));
  gtk_widget_destroy (confirm_dialog);
  if (response == GTK_RESPONSE_CANCEL)
    return FALSE;

  switch (type) {
    case THEME_TYPE_GTK:
      theme = gnome_theme_info_find (name);
      theme_dir = g_build_filename (((GnomeThemeInfo *) theme)->path, "gtk-2.0", NULL);
      break;

    case THEME_TYPE_ICON:
      theme = gnome_theme_icon_info_find (name);
      theme_dir = g_path_get_dirname (((GnomeThemeIconInfo *) theme)->path);
      break;

    case THEME_TYPE_WINDOW:
      theme = gnome_theme_info_find (name);
      theme_dir = g_build_filename (((GnomeThemeInfo *) theme)->path, "metacity-1", NULL);
      break;

    case THEME_TYPE_META:
      theme = gnome_theme_meta_info_find (name);
      theme_dir = g_strdup (((GnomeThemeMetaInfo *) theme)->path);
      break;

    default:
      return FALSE;
  }

  uri_list = g_list_prepend (NULL, gnome_vfs_uri_new (theme_dir));
  g_free (theme_dir);

  result = gnome_vfs_xfer_delete_list (uri_list,
				       GNOME_VFS_XFER_ERROR_MODE_ABORT,
				       GNOME_VFS_XFER_RECURSIVE,
				       NULL, NULL);

  gnome_vfs_uri_list_free (uri_list);

  if (result != GNOME_VFS_OK) {
    GtkWidget *info_dialog = gtk_message_dialog_new (NULL,
						     GTK_DIALOG_MODAL,
						     GTK_MESSAGE_ERROR,
						     GTK_BUTTONS_OK,
						     _("Theme cannot be deleted"));
    gtk_dialog_run (GTK_DIALOG (info_dialog));
    gtk_widget_destroy (info_dialog);
    return FALSE;
  }

  return TRUE;
}

gboolean
theme_model_iter_last (GtkTreeModel *model, GtkTreeIter *iter)
{
  GtkTreeIter walk, prev;
  gboolean valid;

  valid = gtk_tree_model_get_iter_first (model, &walk);

  if (valid) {
    do {
      prev = walk;
      valid = gtk_tree_model_iter_next (model, &walk);
    } while (valid);

    *iter = prev;
    return TRUE;
  }
  return FALSE;
}
