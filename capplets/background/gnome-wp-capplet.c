/*
 *  Authors: Rodney Dawes <dobey@ximian.com>
 *
 *  Copyright 2003 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#include "gnome-wp-capplet.h"

enum {
  TARGET_URI_LIST,
  TARGET_URL,
  TARGET_COLOR,
  TARGET_BGIMAGE,
  TARGET_BACKGROUND_RESET
};

static GtkTargetEntry drop_types[] = {
  {"text/uri-list", 0, TARGET_URI_LIST},
  /*  { "application/x-color", 0, TARGET_COLOR }, */
  { "property/bgimage", 0, TARGET_BGIMAGE },
  /*  { "x-special/gnome-reset-background", 0, TARGET_BACKGROUND_RESET }*/
};

static void wp_props_load_wallpaper (gchar * key,
				     GnomeWPItem * item,
				     GnomeWPCapplet * capplet);

static void wp_properties_error_dialog (GtkWindow * parent, char const * msg,
					GError * err) {
  if (err != NULL) {
    GtkWidget *dialog;

    dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
				     GTK_DIALOG_DESTROY_WITH_PARENT,
				     GTK_MESSAGE_ERROR,
				     GTK_BUTTONS_CLOSE,
				     msg, err->message);

    g_signal_connect (G_OBJECT (dialog),
		      "response",
		      G_CALLBACK (gtk_widget_destroy), NULL);
    gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
    gtk_widget_show (dialog);
    g_error_free (err);
  }
}

static void wp_properties_help (GtkWindow * parent, char const * helpfile,
				char const * section) {
  GError *error = NULL;

  g_return_if_fail (helpfile != NULL);
  g_return_if_fail (section != NULL);

  gnome_help_display_desktop (NULL, "user-guide", helpfile, section, &error);
  if (error != NULL) {
    wp_properties_error_dialog (parent, 
				_("There was an error displaying help: %s"),
				error);
  }
}

static void gnome_wp_file_open_cancel (GtkWidget * widget, gpointer data) {
  gtk_widget_hide (gtk_widget_get_toplevel (widget));
}

static void gnome_wp_file_open_get_files (GtkWidget * widget,
					  GnomeWPCapplet * capplet) {
  GtkWidget * filesel;
  GdkColor color1, color2;
  gchar ** files;
  GdkCursor * cursor;
  gint i;

  cursor = gdk_cursor_new_for_display (gdk_display_get_default (),
				       GDK_WATCH);
  gdk_window_set_cursor (capplet->window->window, cursor);
  gdk_cursor_unref (cursor);

  filesel = gtk_widget_get_toplevel (widget);

  files = gtk_file_selection_get_selections (GTK_FILE_SELECTION (filesel));

  for (i = 0; files && files[i]; i++) {
    GnomeWPItem * item;

    item = g_hash_table_lookup (capplet->wphash, files[i]);
    if (item != NULL) {
      GtkTreePath * path;

      path = gtk_tree_row_reference_get_path (item->rowref);
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (capplet->treeview), path,
				NULL, FALSE);
      gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (capplet->treeview),
				    path, NULL, TRUE, 0.5, 0.0);
      gtk_tree_path_free (path);

      continue;
    }

   item = g_new0 (GnomeWPItem, 1);

   item->filename = g_strdup (files[i]);

   item->fileinfo = gnome_wp_info_new (item->filename, capplet->thumbs);

   item->shade_type = gconf_client_get_string (capplet->client,
					       WP_SHADING_KEY, NULL);
   item->pri_color = gconf_client_get_string (capplet->client,
					      WP_PCOLOR_KEY, NULL);
   item->sec_color = gconf_client_get_string (capplet->client,
					      WP_SCOLOR_KEY, NULL);

   gdk_color_parse (item->pri_color, &color1);
   gdk_color_parse (item->sec_color, &color2);
     
   item->pcolor = gdk_color_copy (&color1);
   item->scolor = gdk_color_copy (&color2);

   if (!strncmp (item->fileinfo->mime_type, "image/", strlen ("image/"))) {
     if (item->name == NULL) {
       item->name = g_strdup (item->fileinfo->name);
     }
     item->options = gconf_client_get_string (capplet->client,
					      WP_OPTIONS_KEY,
					      NULL);

     item->description = g_strdup_printf ("<b>%s</b>\n%s (%LuK)",
					  item->name,
					  gnome_vfs_mime_get_description (item->fileinfo->mime_type),
					  item->fileinfo->size / 1024);
     
     g_hash_table_insert (capplet->wphash, g_strdup (item->filename), item);
     wp_props_load_wallpaper (item->filename, item, capplet);
   } else {
     gnome_wp_item_free (item);
   }
  }

  g_strfreev (files);
  gtk_widget_hide (filesel);

  gdk_window_set_cursor (capplet->window->window, NULL);
}

static void gnome_wp_file_open_dialog (GtkWidget * widget,
				       GnomeWPCapplet * capplet) {
  static GtkWidget * filesel = NULL;

  if (filesel != NULL) {
    gtk_widget_show (filesel);
    return;
  }

  filesel = gtk_file_selection_new (_("Add Wallpapers"));
  gtk_file_selection_set_select_multiple (GTK_FILE_SELECTION (filesel), TRUE);

  g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (filesel)->ok_button),
		    "clicked",
		    G_CALLBACK (gnome_wp_file_open_get_files), capplet);
  g_signal_connect (G_OBJECT (GTK_FILE_SELECTION (filesel)->cancel_button),
		    "clicked",
		    G_CALLBACK (gnome_wp_file_open_cancel), capplet);

  gtk_widget_show (filesel);
}

static void bg_add_multiple_files (GnomeVFSURI * uri,
				   GnomeWPCapplet * capplet) {
  GnomeWPItem * item;
  GdkColor color1, color2;

  item = g_hash_table_lookup (capplet->wphash, gnome_vfs_uri_get_path (uri));
  if (item != NULL) {
    return;
  }

  item = g_new0 (GnomeWPItem, 1);

  item->filename = gnome_vfs_unescape_string_for_display (gnome_vfs_uri_get_path (uri));
  item->fileinfo = gnome_wp_info_new (item->filename, capplet->thumbs);
  item->name = g_strdup (item->fileinfo->name);
  item->options = gconf_client_get_string (capplet->client,
					   WP_OPTIONS_KEY,
					   NULL);

  item->shade_type = gconf_client_get_string (capplet->client,
					      WP_SHADING_KEY, NULL);
  item->pri_color = gconf_client_get_string (capplet->client,
					     WP_PCOLOR_KEY, NULL);
  item->sec_color = gconf_client_get_string (capplet->client,
					     WP_SCOLOR_KEY, NULL);

  gdk_color_parse (item->pri_color, &color1);
  gdk_color_parse (item->sec_color, &color2);
     
  item->pcolor = gdk_color_copy (&color1);
  item->scolor = gdk_color_copy (&color2);

  item->description = g_strdup_printf ("<b>%s</b>\n%s (%LuK)",
				       item->name,
				       gnome_vfs_mime_get_description (item->fileinfo->mime_type),
				       item->fileinfo->size / 1024);
  
  g_hash_table_insert (capplet->wphash, g_strdup (item->filename), item);
  wp_props_load_wallpaper (item->filename, item, capplet);
}

static void bg_properties_dragged_image (GtkWidget * widget,
					 GdkDragContext * context,
					 gint x, gint y,
					 GtkSelectionData * selection_data,
					 guint info, guint time,
					 GnomeWPCapplet * capplet) {

  if (info == TARGET_URI_LIST || info == TARGET_BGIMAGE) {
    GList * uris;

    uris = gnome_vfs_uri_list_parse ((gchar *) selection_data->data);

    if (uris != NULL && uris->data != NULL) {
      if (g_list_length (uris) == 1) {
	GnomeVFSURI * uri = (GnomeVFSURI *) uris->data;
	GnomeWPItem * item;
	GtkTreePath * path;

	item = g_hash_table_lookup (capplet->wphash,
				    gnome_vfs_uri_get_path (uri));

	if (item == NULL) {
	  GdkColor color1, color2;

	  item = g_new0 (GnomeWPItem, 1);

	  item->filename = gnome_vfs_unescape_string_for_display (gnome_vfs_uri_get_path (uri));
	  item->fileinfo = gnome_wp_info_new (item->filename, capplet->thumbs);
	  item->name = g_strdup (item->fileinfo->name);
	  item->options = gconf_client_get_string (capplet->client,
						   WP_OPTIONS_KEY,
						   NULL);
	
	  item->shade_type = gconf_client_get_string (capplet->client,
						      WP_SHADING_KEY, NULL);
	  item->pri_color = gconf_client_get_string (capplet->client,
						     WP_PCOLOR_KEY, NULL);
	  item->sec_color = gconf_client_get_string (capplet->client,
						     WP_SCOLOR_KEY, NULL);

	  gdk_color_parse (item->pri_color, &color1);
	  gdk_color_parse (item->sec_color, &color2);
     
	  item->pcolor = gdk_color_copy (&color1);
	  item->scolor = gdk_color_copy (&color2);

	  item->description = g_strdup_printf ("<b>%s</b>\n%s (%LuK)",
					       item->name,
					       gnome_vfs_mime_get_description (item->fileinfo->mime_type),
					       item->fileinfo->size / 1024);

	  g_hash_table_insert (capplet->wphash, g_strdup (item->filename),
			       item);
	  wp_props_load_wallpaper (item->filename, item, capplet);
	}
	gconf_client_set_string (capplet->client, WP_FILE_KEY,
				 item->filename, NULL);
	gconf_client_set_string (capplet->client, WP_OPTIONS_KEY,
				 item->options, NULL);

	path = gtk_tree_row_reference_get_path (item->rowref);
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (capplet->treeview), path,
				  NULL, FALSE);
	gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (capplet->treeview),
				      path, NULL, TRUE, 0.5, 0.0);
	gtk_tree_path_free (path);
      } else if (g_list_length (uris) > 1) {
	GdkCursor * cursor;

	cursor = gdk_cursor_new_for_display (gdk_display_get_default (),
					     GDK_WATCH);
	gdk_window_set_cursor (capplet->window->window, cursor);
	gdk_cursor_unref (cursor);

	g_list_foreach (uris, (GFunc) bg_add_multiple_files, capplet);

	gdk_window_set_cursor (capplet->window->window, NULL);
      }
    }
    gnome_vfs_uri_list_free (uris);
  }
}

static void wp_props_load_wallpaper (gchar * key,
				     GnomeWPItem * item,
				     GnomeWPCapplet * capplet) {
  GtkTreeIter iter;
  GtkTreePath * path;
  GdkPixbuf * pixbuf;

  if (item->deleted == TRUE) {
    return;
  }

  gtk_list_store_append (GTK_LIST_STORE (capplet->model), &iter);

  pixbuf = gnome_wp_item_get_thumbnail (item, capplet->thumbs);

  if (pixbuf != NULL) {
    gtk_list_store_set (GTK_LIST_STORE (capplet->model), &iter,
			0, pixbuf,
			1, item->description,
			2, item->filename,
			-1);
    g_object_unref (pixbuf);
  } else {
    gtk_list_store_set (GTK_LIST_STORE (capplet->model), &iter,
			1, item->description,
			2, item->filename,
			-1);
  }
  path = gtk_tree_model_get_path (capplet->model, &iter);
  item->rowref = gtk_tree_row_reference_new (capplet->model, path);
  gtk_tree_path_free (path);
}

static gint gnome_wp_option_menu_get (GtkOptionMenu * menu) {
  GtkWidget * widget;

  g_return_val_if_fail (GTK_IS_OPTION_MENU (menu), -1);

  widget = gtk_menu_get_active (GTK_MENU (menu->menu));

  if (widget != NULL) {
    return g_list_index (GTK_MENU_SHELL (menu->menu)->children, widget);
  }

  return -1;
}

static void gnome_wp_option_menu_set (GnomeWPCapplet * capplet,
				      const gchar * value,
				      gboolean shade_type) {
  if (shade_type) {
    if (!strcmp (value, "horizontal-gradient")) {
      gtk_option_menu_set_history (GTK_OPTION_MENU (capplet->color_opt),
				   GNOME_WP_SHADE_TYPE_HORIZ);
      gtk_widget_show (capplet->sc_picker);
    } else if (!strcmp (value, "vertical-gradient")) {
      gtk_option_menu_set_history (GTK_OPTION_MENU (capplet->color_opt),
				   GNOME_WP_SHADE_TYPE_VERT);
      gtk_widget_show (capplet->sc_picker);
    } else {
      gtk_option_menu_set_history (GTK_OPTION_MENU (capplet->color_opt),
				   GNOME_WP_SHADE_TYPE_SOLID);
      gtk_widget_hide (capplet->sc_picker);
    }
  } else {
    if (!strcmp (value, "centered")) {
      gtk_option_menu_set_history (GTK_OPTION_MENU (capplet->wp_opts),
				   GNOME_WP_SCALE_TYPE_CENTERED);
    } else if (!strcmp (value, "stretched")) {
      gtk_option_menu_set_history (GTK_OPTION_MENU (capplet->wp_opts),
				   GNOME_WP_SCALE_TYPE_STRETCHED);
    } else if (!strcmp (value, "scaled")) {
      gtk_option_menu_set_history (GTK_OPTION_MENU (capplet->wp_opts),
				   GNOME_WP_SCALE_TYPE_SCALED);
    } else {
      gtk_option_menu_set_history (GTK_OPTION_MENU (capplet->wp_opts),
				   GNOME_WP_SCALE_TYPE_TILED);
    }
  }
}

static gboolean gnome_wp_props_wp_set (GnomeWPCapplet * capplet) {
  GtkTreeIter iter;
  GtkTreeModel * model;
  GtkTreeSelection * selection;
  GnomeWPItem * item;
  gchar * wpfile;
  GdkPixbuf * pixbuf;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (capplet->treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_tree_model_get (model, &iter, 2, &wpfile, -1);

    item = g_hash_table_lookup (capplet->wphash, wpfile);

    if (!strcmp (item->filename, "(none)")) {
      gconf_client_set_string (capplet->client, WP_OPTIONS_KEY,
			       "none", NULL);
      gtk_widget_set_sensitive (capplet->wp_opts, FALSE);
      gtk_widget_set_sensitive (capplet->rm_button, FALSE);
    } else {
      gtk_widget_set_sensitive (capplet->wp_opts, TRUE);
      gtk_widget_set_sensitive (capplet->rm_button, TRUE);
      gconf_client_set_string (capplet->client, WP_FILE_KEY,
			       item->filename, NULL);
      gconf_client_set_string (capplet->client, WP_OPTIONS_KEY,
			       item->options, NULL);
      gnome_wp_option_menu_set (capplet, item->options, FALSE);
    }

    gconf_client_set_string (capplet->client, WP_SHADING_KEY,
			     item->shade_type, NULL);
    gnome_wp_option_menu_set (capplet, item->shade_type, TRUE);

    gconf_client_set_string (capplet->client, WP_PCOLOR_KEY,
			     item->pri_color, NULL);
    gconf_client_set_string (capplet->client, WP_SCOLOR_KEY,
			     item->sec_color, NULL);

    gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (capplet->pc_picker),
				item->pcolor->red,
				item->pcolor->green,
				item->pcolor->blue, 65535);
    gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (capplet->sc_picker),
				item->scolor->red,
				item->scolor->green,
				item->scolor->blue, 65535);

    g_free (wpfile);

    pixbuf = gnome_wp_pixbuf_new_solid (item->pcolor, 14, 12);
    gtk_image_set_from_pixbuf (GTK_IMAGE (capplet->smenuitem), pixbuf);
    g_object_unref (pixbuf);

    pixbuf = gnome_wp_pixbuf_new_gradient (GTK_ORIENTATION_HORIZONTAL,
					   item->pcolor, item->scolor, 14, 12);
    gtk_image_set_from_pixbuf (GTK_IMAGE (capplet->hmenuitem), pixbuf);
    g_object_unref (pixbuf);
    
    pixbuf = gnome_wp_pixbuf_new_gradient (GTK_ORIENTATION_VERTICAL,
					   item->pcolor, item->scolor, 14, 12);
    gtk_image_set_from_pixbuf (GTK_IMAGE (capplet->vmenuitem), pixbuf);
    g_object_unref (pixbuf);
  } else {
    gtk_widget_set_sensitive (capplet->rm_button, FALSE);
  }

  return FALSE;
}

static void gnome_wp_props_wp_selected (GtkTreeSelection * selection,
					GnomeWPCapplet * capplet) {
  if (capplet->idleid > 0) {
    g_source_remove (capplet->idleid);
  }
  capplet->idleid = g_timeout_add (capplet->delay + 100,
				   (GSourceFunc) gnome_wp_props_wp_set,
				   capplet);
}

static void gnome_wp_remove_wp (gchar * key, GnomeWPItem * item,
				GnomeWPCapplet * capplet) {
  GtkTreePath * path;
  GtkTreeIter iter;

  if (item->rowref != NULL && item->deleted == FALSE) {
    path = gtk_tree_row_reference_get_path (item->rowref);
    gtk_tree_model_get_iter (capplet->model, &iter, path);
    gtk_tree_path_free (path);

    gtk_list_store_remove (GTK_LIST_STORE (capplet->model), &iter);
  }
}

void gnome_wp_main_quit (GnomeWPCapplet * capplet) {
    g_hash_table_foreach (capplet->wphash, (GHFunc) gnome_wp_remove_wp,
			  capplet);

    gnome_wp_xml_save_list (capplet);

    g_object_unref (capplet->thumbs);

    g_hash_table_destroy (capplet->wphash);

    gtk_main_quit ();
}

static void wallpaper_properties_clicked (GtkWidget * dialog,
					  gint response_id,
					  GnomeWPCapplet * capplet) {
  switch (response_id) {
  case GTK_RESPONSE_HELP:
    wp_properties_help (GTK_WINDOW (dialog),
			"wgoscustdesk.xml", "goscustdesk-7");
    break;
  case GTK_RESPONSE_CLOSE: {
    gtk_widget_destroy (dialog);
    gnome_wp_main_quit (capplet);
    break;
  }
  }
}

static void gnome_wp_scale_type_changed (GtkMenuShell * shell,
					 GnomeWPCapplet * capplet) {
  GnomeWPItem * item = NULL;
  GtkTreeIter iter;
  GtkTreeModel * model;
  GtkTreeSelection * selection;
  gchar * wpfile;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (capplet->treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_tree_model_get (model, &iter, 2, &wpfile, -1);

    item = g_hash_table_lookup (capplet->wphash, wpfile);

    g_free (wpfile);
  }

  if (item == NULL) {
    return;
  }
  switch (gnome_wp_option_menu_get (GTK_OPTION_MENU (capplet->wp_opts))) {
  case GNOME_WP_SCALE_TYPE_CENTERED:
    item->options = g_strdup ("centered");
    break;
  case GNOME_WP_SCALE_TYPE_STRETCHED:
    item->options = g_strdup ("stretched");
    break;
  case GNOME_WP_SCALE_TYPE_SCALED:
    item->options = g_strdup ("scaled");
    break;
  case GNOME_WP_SCALE_TYPE_TILED:
    item->options = g_strdup ("wallpaper");
    break;
  default:
    break;
  }
  gconf_client_set_string (capplet->client, WP_OPTIONS_KEY,
			   item->options, NULL);
}

static void gnome_wp_shade_type_changed (GtkMenuShell * shell,
					 GnomeWPCapplet * capplet) {
  GnomeWPItem * item = NULL;
  GtkTreeIter iter;
  GtkTreeModel * model;
  GtkTreeSelection * selection;
  gchar * wpfile;
  GdkPixbuf * pixbuf;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (capplet->treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_tree_model_get (model, &iter, 2, &wpfile, -1);

    item = g_hash_table_lookup (capplet->wphash, wpfile);

    g_free (wpfile);
  }

  if (item == NULL) {
    return;
  }

  switch (gnome_wp_option_menu_get (GTK_OPTION_MENU (capplet->color_opt))) {
  case GNOME_WP_SHADE_TYPE_HORIZ:
    item->shade_type = g_strdup ("horizontal-gradient");
    gtk_widget_show (capplet->sc_picker);
    break;
  case GNOME_WP_SHADE_TYPE_VERT:
    item->shade_type = g_strdup ("vertical-gradient");
    gtk_widget_show (capplet->sc_picker);
    break;
  case GNOME_WP_SHADE_TYPE_SOLID:
    item->shade_type = g_strdup ("solid");
    gtk_widget_hide (capplet->sc_picker);
    break;
  default:
    break;
  }
  pixbuf = gnome_wp_item_get_thumbnail (item, capplet->thumbs);
  gtk_list_store_set (GTK_LIST_STORE (capplet->model), &iter,
		      0, pixbuf,
		      -1);
  g_object_unref (pixbuf);
  gconf_client_set_string (capplet->client, WP_SHADING_KEY,
			   item->shade_type, NULL);
}

static void gnome_wp_color_changed (GnomeWPCapplet * capplet,
				    gboolean primary) {
  GnomeWPItem * item = NULL;
  GtkTreeIter iter;
  GtkTreeModel * model;
  GtkTreeSelection * selection;
  gchar * wpfile;
  GdkPixbuf * pixbuf;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (capplet->treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_tree_model_get (model, &iter, 2, &wpfile, -1);

    item = g_hash_table_lookup (capplet->wphash, wpfile);

    g_free (wpfile);
  }

  if (item == NULL) {
    return;
  }

  if (primary) {
    gnome_color_picker_get_i16 (GNOME_COLOR_PICKER (capplet->pc_picker),
				&item->pcolor->red,
				&item->pcolor->green,
				&item->pcolor->blue, NULL);
    item->pri_color = g_strdup_printf ("#%02X%02X%02X",
				       item->pcolor->red >> 8,
				       item->pcolor->green >> 8,
				       item->pcolor->blue >> 8);
    gconf_client_set_string (capplet->client, WP_PCOLOR_KEY,
			     item->pri_color, NULL);
  } else {
    gnome_color_picker_get_i16 (GNOME_COLOR_PICKER (capplet->sc_picker),
				&item->scolor->red,
				&item->scolor->green,
				&item->scolor->blue, NULL);
    item->sec_color = g_strdup_printf ("#%02X%02X%02X",
				       item->scolor->red >> 8,
				       item->scolor->green >> 8,
				       item->scolor->blue >> 8);
    gconf_client_set_string (capplet->client, WP_SCOLOR_KEY,
			     item->sec_color, NULL);
  }

  gnome_wp_shade_type_changed (NULL, capplet);

  pixbuf = gnome_wp_pixbuf_new_solid (item->pcolor, 14, 12);
  gtk_image_set_from_pixbuf (GTK_IMAGE (capplet->smenuitem), pixbuf);
  g_object_unref (pixbuf);

  pixbuf = gnome_wp_pixbuf_new_gradient (GTK_ORIENTATION_HORIZONTAL,
					 item->pcolor, item->scolor, 14, 12);
  gtk_image_set_from_pixbuf (GTK_IMAGE (capplet->hmenuitem), pixbuf);
  g_object_unref (pixbuf);

  pixbuf = gnome_wp_pixbuf_new_gradient (GTK_ORIENTATION_VERTICAL,
					 item->pcolor, item->scolor, 14, 12);
  gtk_image_set_from_pixbuf (GTK_IMAGE (capplet->vmenuitem), pixbuf);
  g_object_unref (pixbuf);
}

static void gnome_wp_pcolor_changed (GtkWidget * widget,
				     guint r, guint g, guint b, guint a,
				     GnomeWPCapplet * capplet) {
  gnome_wp_color_changed (capplet, TRUE);
}

static void gnome_wp_scolor_changed (GtkWidget * widget,
				     guint r, guint g, guint b, guint a,
				     GnomeWPCapplet * capplet) {
  gnome_wp_color_changed (capplet, FALSE);
}

static void gnome_wp_remove_wallpaper (GtkWidget * widget,
				       GnomeWPCapplet * capplet) {
  GtkTreeIter iter;
  GtkTreeModel * model;
  GtkTreeSelection * selection;
  gchar * wpfile;

  if (capplet->idleid > 0) {
    g_source_remove (capplet->idleid);
  }

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (capplet->treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    GnomeWPItem * item;

    gtk_tree_model_get (model, &iter, 2, &wpfile, -1);

    item = g_hash_table_lookup (capplet->wphash, wpfile);
    item->deleted = TRUE;

    gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
  }
}

static gboolean gnome_wp_load_stuffs (void * data) {
  GnomeWPCapplet * capplet = (GnomeWPCapplet *) data;
  gchar * imagepath;
  GnomeWPItem * item;
  GtkTreePath * path;

  gnome_wp_xml_load_list (capplet);
  g_hash_table_foreach (capplet->wphash, (GHFunc) wp_props_load_wallpaper,
			capplet);

  gdk_window_set_cursor (capplet->window->window, NULL);
  
  imagepath = gconf_client_get_string (capplet->client,
				       WP_FILE_KEY,
				       NULL);
  item = g_hash_table_lookup (capplet->wphash, imagepath);
  if (item != NULL) {
    if (item->deleted == TRUE) {
      item->deleted = FALSE;
      wp_props_load_wallpaper (item->filename, item, capplet);
    }

    path = gtk_tree_row_reference_get_path (item->rowref);
    gtk_tree_view_set_cursor (GTK_TREE_VIEW (capplet->treeview), path,
			      NULL, FALSE);
    gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (capplet->treeview),
				  path, NULL, TRUE, 0.5, 0.0);
    gtk_tree_path_free (path);

    gnome_wp_option_menu_set (capplet, item->options, FALSE);
    gnome_wp_option_menu_set (capplet, item->shade_type, TRUE);

    gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (capplet->pc_picker),
				item->pcolor->red,
				item->pcolor->green,
				item->pcolor->blue, 65535);
    gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (capplet->sc_picker),
				item->scolor->red,
				item->scolor->green,
				item->scolor->blue, 65535);
  } else {
    GdkColor color1, color2;

    item = g_new0 (GnomeWPItem, 1);

    item->filename = g_strdup (imagepath);

    item->shade_type = gconf_client_get_string (capplet->client,
						WP_SHADING_KEY, NULL);
    item->pri_color = gconf_client_get_string (capplet->client,
					       WP_PCOLOR_KEY, NULL);
    item->sec_color = gconf_client_get_string (capplet->client,
					       WP_SCOLOR_KEY, NULL);

    gdk_color_parse (item->pri_color, &color1);
    gdk_color_parse (item->sec_color, &color2);

    item->pcolor = gdk_color_copy (&color1);
    item->scolor = gdk_color_copy (&color2);

    if (g_file_test (item->filename, G_FILE_TEST_EXISTS)) {
      item->fileinfo = gnome_wp_info_new (item->filename, capplet->thumbs);
      item->name = g_strdup (item->fileinfo->name);
      item->options = gconf_client_get_string (capplet->client,
					       WP_OPTIONS_KEY,
					       NULL);
      item->deleted = FALSE;

      item->description = g_strdup_printf ("<b>%s</b>\n%s (%LuK)",
					   item->name,
					   gnome_vfs_mime_get_description (item->fileinfo->mime_type),
					   item->fileinfo->size / 1024);

      g_hash_table_insert (capplet->wphash, g_strdup (item->filename), item);
      wp_props_load_wallpaper (item->filename, item, capplet);

      path = gtk_tree_row_reference_get_path (item->rowref);
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (capplet->treeview), path,
				NULL, FALSE);
      gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (capplet->treeview),
				    path, NULL, TRUE, 0.5, 0.0);
      gtk_tree_path_free (path);

      gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (capplet->pc_picker),
				  item->pcolor->red,
				  item->pcolor->green,
				  item->pcolor->blue, 65535);
      gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (capplet->sc_picker),
				  item->scolor->red,
				  item->scolor->green,
				  item->scolor->blue, 65535);

      gnome_wp_option_menu_set (capplet, item->options, FALSE);
      gnome_wp_option_menu_set (capplet, item->shade_type, TRUE);
    } else {
      gnome_wp_item_free (item);
    }
  }
  g_free (imagepath);

  item = g_hash_table_lookup (capplet->wphash, "(none)");
  if (item == NULL) {
    GdkColor color1, color2;

    item = g_new0 (GnomeWPItem, 1);

    item->deleted = FALSE;
    item->filename = g_strdup ("(none)");

    item->shade_type = gconf_client_get_string (capplet->client,
						WP_SHADING_KEY, NULL);
    item->pri_color = gconf_client_get_string (capplet->client,
					       WP_PCOLOR_KEY, NULL);
    item->sec_color = gconf_client_get_string (capplet->client,
					       WP_SCOLOR_KEY, NULL);

    gdk_color_parse (item->pri_color, &color1);
    gdk_color_parse (item->sec_color, &color2);

    item->pcolor = gdk_color_copy (&color1);
    item->scolor = gdk_color_copy (&color2);

    item->fileinfo = gnome_wp_info_new (item->filename, capplet->thumbs);
    item->name = g_strdup (item->fileinfo->name);
    item->options = gconf_client_get_string (capplet->client,
					     WP_OPTIONS_KEY,
					     NULL);

    item->description = g_strdup_printf ("<b>%s</b>", item->name);

    g_hash_table_insert (capplet->wphash, g_strdup (item->filename), item);
    wp_props_load_wallpaper (item->filename, item, capplet);
  } else {
    if (item->deleted == TRUE) {
      item->deleted = FALSE;
      wp_props_load_wallpaper (item->filename, item, capplet);
    }
  }

  return FALSE;
}

static gint gnome_wp_list_sort (GtkTreeModel * model,
				GtkTreeIter * a, GtkTreeIter * b,
				GnomeWPCapplet * capplet) {
  gchar * foo, * bar;
  gchar * desca, * descb;

  gtk_tree_model_get (model, a, 1, &desca, 2, &foo, -1);
  gtk_tree_model_get (model, b, 1, &descb, 2, &bar, -1);

  if (!strcmp (foo, "(none)")) {
    return -1;
  } else if (!strcmp (bar, "(none)")) {
    return 1;
  } else {
    return strcmp (desca, descb);
  }
}

static void gnome_wp_file_changed (GConfClient * client, guint id,
				   GConfEntry * entry,
				   GnomeWPCapplet * capplet) {
  GtkTreeSelection * selection;
  GtkTreeModel * model;
  GtkTreeIter iter;
  GtkTreePath * path;
  GnomeWPItem * item;
  gchar * wpfile, * selected;

  wpfile = g_strdup (gconf_value_get_string (entry->value));
  item = g_hash_table_lookup (capplet->wphash, wpfile);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (capplet->treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_tree_model_get (model, &iter, 2, &selected, -1);
    if (strcmp (selected, wpfile) != 0) {
      if (item != NULL) {
	path = gtk_tree_row_reference_get_path (item->rowref);
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (capplet->treeview), path,
				  NULL, FALSE);
	gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (capplet->treeview),
				      path, NULL, TRUE, 0.5, 0.0);
	gtk_tree_path_free (path);
      } else {
	GdkColor color1, color2;

	item = g_new0 (GnomeWPItem, 1);

	item->filename = g_strdup (wpfile);

	item->shade_type = gconf_client_get_string (capplet->client,
						    WP_SHADING_KEY, NULL);
	item->pri_color = gconf_client_get_string (capplet->client,
						   WP_PCOLOR_KEY, NULL);
	item->sec_color = gconf_client_get_string (capplet->client,
						   WP_SCOLOR_KEY, NULL);
    
	gdk_color_parse (item->pri_color, &color1);
	gdk_color_parse (item->sec_color, &color2);
    
	item->pcolor = gdk_color_copy (&color1);
	item->scolor = gdk_color_copy (&color2);
    
	if (g_file_test (item->filename, G_FILE_TEST_EXISTS)) {
	  item->fileinfo = gnome_wp_info_new (item->filename, capplet->thumbs);
	  item->name = g_strdup (item->fileinfo->name);
	  item->options = gconf_client_get_string (capplet->client,
						   WP_OPTIONS_KEY,
						   NULL);
      
	  item->description = g_strdup_printf ("<b>%s</b>\n%s (%LuK)",
					       item->name,
					       gnome_vfs_mime_get_description (item->fileinfo->mime_type),
					       item->fileinfo->size / 1024);
      
	  g_hash_table_insert (capplet->wphash,
			       g_strdup (item->filename), item);
	  wp_props_load_wallpaper (item->filename, item, capplet);
      
	  path = gtk_tree_row_reference_get_path (item->rowref);
	  gtk_tree_view_set_cursor (GTK_TREE_VIEW (capplet->treeview), path,
				    NULL, FALSE);
	  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (capplet->treeview),
					path, NULL, TRUE, 0.5, 0.0);
	  gtk_tree_path_free (path);
	} else {
	  gnome_wp_item_free (item);
	}
      }
    }
    g_free (wpfile);
    g_free (selected);
  }
}

static void gnome_wp_options_changed (GConfClient * client, guint id,
				      GConfEntry * entry,
				      GnomeWPCapplet * capplet) {
  GtkTreeSelection * selection;
  GtkTreeModel * model;
  GtkTreeIter iter;
  GnomeWPItem * item;
  gchar * wpfile;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (capplet->treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_tree_model_get (model, &iter, 2, &wpfile, -1);

    item = g_hash_table_lookup (capplet->wphash, wpfile);

    if (item != NULL) {
      item->options = g_strdup (gconf_value_get_string (entry->value));
      gnome_wp_option_menu_set (capplet, item->options, FALSE);
    }
    g_free (wpfile);
  }
}

static void gnome_wp_shading_changed (GConfClient * client, guint id,
				      GConfEntry * entry,
				      GnomeWPCapplet * capplet) {
  GtkTreeSelection * selection;
  GtkTreeModel * model;
  GtkTreeIter iter;
  GnomeWPItem * item;
  gchar * wpfile;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (capplet->treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_tree_model_get (model, &iter, 2, &wpfile, -1);

    item = g_hash_table_lookup (capplet->wphash, wpfile);

    if (item != NULL) {
      item->shade_type = g_strdup (gconf_value_get_string (entry->value));
      gnome_wp_option_menu_set (capplet, item->shade_type, TRUE);
    }
    g_free (wpfile);
  }
}

static void gnome_wp_color1_changed (GConfClient * client, guint id,
				     GConfEntry * entry,
				     GnomeWPCapplet * capplet) {
  GdkColor color;
  const gchar * colorhex;

  colorhex = gconf_value_get_string (entry->value);

  gdk_color_parse (colorhex, &color);

  gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (capplet->pc_picker),
			      color.red,
			      color.green,
			      color.blue, 65535);

  gnome_wp_color_changed (capplet, TRUE);
}

static void gnome_wp_color2_changed (GConfClient * client, guint id,
				     GConfEntry * entry,
				     GnomeWPCapplet * capplet) {
  GdkColor color;
  const gchar * colorhex;

  colorhex = gconf_value_get_string (entry->value);

  gdk_color_parse (colorhex, &color);

  gnome_color_picker_set_i16 (GNOME_COLOR_PICKER (capplet->sc_picker),
			      color.red,
			      color.green,
			      color.blue, 65535);

  gnome_wp_color_changed (capplet, FALSE);
}

static void gnome_wp_delay_changed (GConfClient * client, guint id,
				       GConfEntry * entry,
				       GnomeWPCapplet * capplet) {
  capplet->delay = gconf_value_get_int (entry->value);
}

static void gnome_wp_icon_theme_changed (GnomeIconTheme * theme,
					 GnomeWPCapplet * capplet) {
  gchar * icofile;

  icofile = gnome_icon_theme_lookup_icon (capplet->theme,
					  "background-capplet",
					  48, NULL, NULL);
  if (icofile != NULL) {
    gtk_window_set_default_icon_from_file (icofile, NULL);
  }
  g_free (icofile);

  icofile = gnome_icon_theme_lookup_icon (capplet->theme,
					  "stock_wallpaper-center",
					  16, NULL, NULL);
  if (icofile != NULL) {
    GdkPixbuf * pixbuf;

    pixbuf = gdk_pixbuf_new_from_file (icofile, NULL);
    gtk_image_set_from_pixbuf (GTK_IMAGE (capplet->citem), pixbuf);
    g_object_unref (pixbuf);
  }
  g_free (icofile);

  icofile = gnome_icon_theme_lookup_icon (capplet->theme,
					  "stock_wallpaper-fill",
					  16, NULL, NULL);
  if (icofile != NULL) {
    GdkPixbuf * pixbuf;

    pixbuf = gdk_pixbuf_new_from_file (icofile, NULL);
    gtk_image_set_from_pixbuf (GTK_IMAGE (capplet->fitem), pixbuf);
    g_object_unref (pixbuf);
  }
  g_free (icofile);

  icofile = gnome_icon_theme_lookup_icon (capplet->theme,
					  "stock_wallpaper-scale",
					  16, NULL, NULL);
  if (icofile != NULL) {
    GdkPixbuf * pixbuf;

    pixbuf = gdk_pixbuf_new_from_file (icofile, NULL);
    gtk_image_set_from_pixbuf (GTK_IMAGE (capplet->sitem), pixbuf);
    g_object_unref (pixbuf);
  }
  g_free (icofile);

  icofile = gnome_icon_theme_lookup_icon (capplet->theme,
					  "stock_wallpaper-tile",
					  16, NULL, NULL);
  if (icofile != NULL) {
    GdkPixbuf * pixbuf;

    pixbuf = gdk_pixbuf_new_from_file (icofile, NULL);
    gtk_image_set_from_pixbuf (GTK_IMAGE (capplet->witem), pixbuf);
    g_object_unref (pixbuf);
  }
  g_free (icofile);
}

static void wallpaper_properties_init (void) {
  GnomeWPCapplet * capplet;
  GtkWidget * label, * button;
  GtkWidget * vbox, * hbox, * bbox;
  GtkWidget * swin, * clabel;
  GtkWidget * menu;
  GtkWidget * mbox, * mitem;
  GtkCellRenderer * renderer;
  GtkTreeViewColumn * column;
  GtkTreeSelection * selection;
  GdkCursor * cursor;
  gchar * icofile;

  capplet = g_new0 (GnomeWPCapplet, 1);

  if (capplet->client == NULL) {
    capplet->client = gconf_client_get_default ();
  }

  capplet->delay = gconf_client_get_int (capplet->client,
					 WP_DELAY_KEY,
					 NULL);
  gconf_client_add_dir (capplet->client, WP_KEYBOARD_PATH,
			GCONF_CLIENT_PRELOAD_NONE, NULL);
  gconf_client_add_dir (capplet->client, WP_PATH_KEY,
			GCONF_CLIENT_PRELOAD_NONE, NULL);

  gconf_client_notify_add (capplet->client,
			   WP_DELAY_KEY,
			   (GConfClientNotifyFunc) gnome_wp_delay_changed,
			   capplet, NULL, NULL);
  gconf_client_notify_add (capplet->client,
			   WP_FILE_KEY,
			   (GConfClientNotifyFunc) gnome_wp_file_changed,
			   capplet, NULL, NULL);
  gconf_client_notify_add (capplet->client,
			   WP_OPTIONS_KEY,
			   (GConfClientNotifyFunc) gnome_wp_options_changed,
			   capplet, NULL, NULL);
  gconf_client_notify_add (capplet->client,
			   WP_SHADING_KEY,
			   (GConfClientNotifyFunc) gnome_wp_shading_changed,
			   capplet, NULL, NULL);
  gconf_client_notify_add (capplet->client,
			   WP_PCOLOR_KEY,
			   (GConfClientNotifyFunc) gnome_wp_color1_changed,
			   capplet, NULL, NULL);
  gconf_client_notify_add (capplet->client,
			   WP_SCOLOR_KEY,
			   (GConfClientNotifyFunc) gnome_wp_color2_changed,
			   capplet, NULL, NULL);

  capplet->wphash = g_hash_table_new_full (g_str_hash, g_str_equal,
					   g_free,
					   (GDestroyNotify)
					   gnome_wp_item_free);

  capplet->thumbs = gnome_thumbnail_factory_new (GNOME_THUMBNAIL_SIZE_NORMAL);
  capplet->theme = gnome_icon_theme_new ();
  gnome_icon_theme_set_allow_svg (capplet->theme, TRUE);

  g_signal_connect (G_OBJECT (capplet->theme), "changed",
		    G_CALLBACK (gnome_wp_icon_theme_changed), capplet);

  capplet->window = gtk_dialog_new ();
  gtk_window_set_title (GTK_WINDOW (capplet->window),
			_("Desktop Wallpaper Preferences"));
  gtk_dialog_set_has_separator (GTK_DIALOG (capplet->window), FALSE);
  gtk_window_set_default_size (GTK_WINDOW (capplet->window), 360, 418);

  icofile = gnome_icon_theme_lookup_icon (capplet->theme,
					  "background-capplet",
					  48, NULL, NULL);
  if (icofile != NULL) {
    gtk_window_set_default_icon_from_file (icofile, NULL);
  }
  g_free (icofile);

  gtk_widget_realize (capplet->window);



  /* Drag and Drop Support */
  gtk_drag_dest_unset (capplet->window);
  gtk_drag_dest_set (capplet->window, GTK_DEST_DEFAULT_ALL, drop_types,
		     sizeof (drop_types) / sizeof (drop_types[0]),
		     GDK_ACTION_COPY | GDK_ACTION_MOVE);
  g_signal_connect (G_OBJECT (capplet->window), "drag_data_received",
		    G_CALLBACK (bg_properties_dragged_image), capplet);

  /* Dialog Buttons */
  label = gtk_button_new_from_stock (GTK_STOCK_HELP);
  gtk_dialog_add_action_widget (GTK_DIALOG (capplet->window), label,
				GTK_RESPONSE_HELP);
  gtk_widget_show (label);

  label = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
  gtk_dialog_add_action_widget (GTK_DIALOG (capplet->window), label,
				GTK_RESPONSE_CLOSE);
  gtk_widget_show (label);

  /* Main Contents */
  vbox = gtk_vbox_new (FALSE, 6);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (capplet->window)->vbox), vbox,
		      TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  clabel = gtk_label_new_with_mnemonic (_("<b>Desktop _Wallpaper</b>"));
  gtk_misc_set_alignment (GTK_MISC (clabel), 0.0, 0.5);
  gtk_label_set_use_markup (GTK_LABEL (clabel), TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), clabel, FALSE, FALSE, 0);
  gtk_widget_show (clabel);

  /* Treeview stuff goes in here */
  hbox = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin),
				       GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (hbox), swin, TRUE, TRUE, 0);

  capplet->treeview = gtk_tree_view_new ();
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (capplet->treeview), FALSE);
  gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (capplet->treeview), FALSE);
  gtk_container_add (GTK_CONTAINER (swin), capplet->treeview);
  gtk_widget_show (capplet->treeview);

  gtk_label_set_mnemonic_widget (GTK_LABEL (clabel), capplet->treeview);

  capplet->model = GTK_TREE_MODEL (gtk_list_store_new (3, GDK_TYPE_PIXBUF,
						       G_TYPE_STRING,
						       G_TYPE_STRING));

  gtk_tree_view_set_model (GTK_TREE_VIEW (capplet->treeview), capplet->model);

  column = gtk_tree_view_column_new ();
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
				       "pixbuf", 0,
				       NULL);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
				       "markup", 1,
				       NULL);
  gtk_tree_view_column_set_spacing (column, 6);

  gtk_tree_view_append_column (GTK_TREE_VIEW (capplet->treeview), column);

  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (capplet->model), 2,
				   (GtkTreeIterCompareFunc) gnome_wp_list_sort,
				   capplet, NULL);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (capplet->model),
					2, GTK_SORT_ASCENDING);

  gtk_rc_parse_string ("style \"wp-tree-defaults\" {\n"
		       "  GtkTreeView::horizontal-separator = 6\n"
		       "  GtkTreeView::vertical-separator = 6\n"
		       "} widget_class \"*TreeView*\""
		       " style \"wp-tree-defaults\"");

  /* Need to add sorting stuff and whatnot */
  gtk_widget_show (swin);

  /* The Box for Fill Style and Add/Remove buttons */
  hbox = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  label = gtk_label_new_with_mnemonic (_("_Style:"));
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  capplet->wp_opts = gtk_option_menu_new ();
  gtk_box_pack_start (GTK_BOX (hbox), capplet->wp_opts, FALSE, FALSE, 0);
  gtk_widget_show (capplet->wp_opts);

  gtk_label_set_mnemonic_widget (GTK_LABEL (label), capplet->wp_opts);

  menu = gtk_menu_new ();
  mitem = gtk_menu_item_new ();
  icofile = gnome_icon_theme_lookup_icon (capplet->theme,
					  "stock_wallpaper-center",
					  16, NULL, NULL);

  mbox = gtk_hbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (mitem), mbox);
  gtk_widget_show (mbox);

  if (icofile != NULL) {
    capplet->citem = gtk_image_new_from_file (icofile);
    gtk_box_pack_start (GTK_BOX (mbox), capplet->citem, FALSE, FALSE, 0);
    gtk_widget_show (capplet->citem);
  }
  g_free (icofile);
    
  label = gtk_label_new (_("Centered"));
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (mbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
  gtk_menu_append (GTK_MENU (menu), mitem);
  gtk_widget_show (mitem);

  mitem = gtk_menu_item_new ();
  icofile = gnome_icon_theme_lookup_icon (capplet->theme,
					  "stock_wallpaper-fill",
					  16, NULL, NULL);
  mbox = gtk_hbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (mitem), mbox);
  gtk_widget_show (mbox);

  if (icofile != NULL) {
    capplet->fitem = gtk_image_new_from_file (icofile);
    gtk_box_pack_start (GTK_BOX (mbox), capplet->fitem, FALSE, FALSE, 0);
    gtk_widget_show (capplet->fitem);
  }
  g_free (icofile);

  label = gtk_label_new (_("Fill Screen"));
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (mbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
  gtk_menu_append (GTK_MENU (menu), mitem);
  gtk_widget_show (mitem);

  mitem = gtk_menu_item_new ();
  icofile = gnome_icon_theme_lookup_icon (capplet->theme,
					  "stock_wallpaper-scale",
					  16, NULL, NULL);
  mbox = gtk_hbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (mitem), mbox);
  gtk_widget_show (mbox);

  if (icofile != NULL) {
    capplet->sitem = gtk_image_new_from_file (icofile);
    gtk_box_pack_start (GTK_BOX (mbox), capplet->sitem, FALSE, FALSE, 0);
    gtk_widget_show (capplet->sitem);
  }
  label = gtk_label_new (_("Scaled"));
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (mbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
  gtk_menu_append (GTK_MENU (menu), mitem);
  gtk_widget_show (mitem);

  mitem = gtk_menu_item_new ();
  icofile = gnome_icon_theme_lookup_icon (capplet->theme,
					  "stock_wallpaper-tile",
					  16, NULL, NULL);
  mbox = gtk_hbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (mitem), mbox);
  gtk_widget_show (mbox);

  if (icofile != NULL) {
    capplet->witem = gtk_image_new_from_file (icofile);
    gtk_box_pack_start (GTK_BOX (mbox), capplet->witem, FALSE, FALSE, 0);
    gtk_widget_show (capplet->witem);
  }
  label = gtk_label_new (_("Tiled"));
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (mbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
  gtk_menu_append (GTK_MENU (menu), mitem);
  gtk_widget_show (mitem);

  gtk_option_menu_set_menu (GTK_OPTION_MENU (capplet->wp_opts), menu);

  g_signal_connect (G_OBJECT (menu), "deactivate",
		    G_CALLBACK (gnome_wp_scale_type_changed), capplet);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /* Create the Remove button first, since it's ordered last */
  capplet->rm_button = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
  gtk_box_pack_end (GTK_BOX (hbox), capplet->rm_button, FALSE, FALSE, 0);
  gtk_widget_show (capplet->rm_button);

  g_signal_connect (G_OBJECT (capplet->rm_button), "clicked",
		    G_CALLBACK (gnome_wp_remove_wallpaper), capplet);

  /* Now do the Add Wallpaper button */
  button = gtk_button_new ();
  gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);

  bbox = gtk_hbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (button), bbox);
  label = gtk_image_new_from_stock (GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON);
  gtk_box_pack_start (GTK_BOX (bbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);
  label = gtk_label_new_with_mnemonic (_("_Add Wallpaper"));
  gtk_box_pack_start (GTK_BOX (bbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);
  gtk_widget_show (bbox);

  gtk_widget_show (button);

  g_signal_connect (G_OBJECT (button), "clicked",
		    G_CALLBACK (gnome_wp_file_open_dialog), capplet);

  /* Silly Random Option */
  /*
  hbox = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);
  */

  /* Stupid Useless Label as a Spacer Hack */
  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /* The Wallpaper Background Colors Section */
  clabel = gtk_label_new_with_mnemonic (_("<b>_Desktop Colors</b> "));
  gtk_misc_set_alignment (GTK_MISC (clabel), 0.0, 0.5);
  gtk_label_set_use_markup (GTK_LABEL (clabel), TRUE);
  gtk_box_pack_start (GTK_BOX (vbox), clabel, FALSE, FALSE, 0);
  gtk_widget_show (clabel);

  hbox = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_widget_show (hbox);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  label = gtk_label_new ("");
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  capplet->color_opt = gtk_option_menu_new ();
  gtk_widget_show (capplet->color_opt);
  gtk_box_pack_start(GTK_BOX (hbox), capplet->color_opt, FALSE, FALSE, 0);

  menu = gtk_menu_new ();
  mitem = gtk_menu_item_new ();
  mbox = gtk_hbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (mitem), mbox);
  gtk_widget_show (mbox);

  capplet->smenuitem = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (mbox), capplet->smenuitem, FALSE, FALSE, 0);
  gtk_widget_show (capplet->smenuitem);

  label = gtk_label_new (_("Solid Color"));
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (mbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
  gtk_widget_show (mitem);
  gtk_menu_append (GTK_MENU (menu), mitem);

  mitem = gtk_menu_item_new ();
  mbox = gtk_hbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (mitem), mbox);
  gtk_widget_show (mbox);

  capplet->hmenuitem = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (mbox), capplet->hmenuitem, FALSE, FALSE, 0);
  gtk_widget_show (capplet->hmenuitem);

  label = gtk_label_new (_("Horizontal Gradient"));
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (mbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
  gtk_widget_show (mitem);
  gtk_menu_append (GTK_MENU (menu), mitem);

  mitem = gtk_menu_item_new ();
  mbox = gtk_hbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (mitem), mbox);
  gtk_widget_show (mbox);

  capplet->vmenuitem = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (mbox), capplet->vmenuitem, FALSE, FALSE, 0);
  gtk_widget_show (capplet->vmenuitem);

  label = gtk_label_new (_("Vertical Gradient"));
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_box_pack_start (GTK_BOX (mbox), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
  gtk_widget_show (mitem);
  gtk_menu_append (GTK_MENU (menu), mitem);

  gtk_option_menu_set_menu (GTK_OPTION_MENU (capplet->color_opt), menu);
  g_signal_connect (G_OBJECT (menu), "deactivate",
		    G_CALLBACK (gnome_wp_shade_type_changed), capplet);

  gtk_label_set_mnemonic_widget (GTK_LABEL (clabel), capplet->color_opt);

  capplet->pc_picker = gnome_color_picker_new ();
  gtk_widget_show (capplet->pc_picker);
  gtk_box_pack_start (GTK_BOX (hbox), capplet->pc_picker, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (capplet->pc_picker), "color_set",
		    G_CALLBACK (gnome_wp_pcolor_changed), capplet);

  capplet->sc_picker = gnome_color_picker_new ();
  gtk_box_pack_start (GTK_BOX (hbox), capplet->sc_picker, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (capplet->sc_picker), "color_set",
		    G_CALLBACK (gnome_wp_scolor_changed), capplet);
  
  g_signal_connect (G_OBJECT (capplet->window), "response",
		    G_CALLBACK (wallpaper_properties_clicked), capplet);

  gtk_widget_show (capplet->window);

  cursor = gdk_cursor_new_for_display (gdk_display_get_default (),
				       GDK_WATCH);
  gdk_window_set_cursor (capplet->window->window, cursor);
  gdk_cursor_unref (cursor);

  g_idle_add (gnome_wp_load_stuffs, capplet);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (capplet->treeview));
  g_signal_connect (G_OBJECT (selection), "changed",
		    G_CALLBACK (gnome_wp_props_wp_selected), capplet);
}

gint main (gint argc, gchar *argv[]) {
  GnomeProgram * proggie;

#ifdef ENABLE_NLS
  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
#endif

  proggie = gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE,
				argc, argv, GNOME_PARAM_POPT_TABLE,
				NULL, NULL);

  wallpaper_properties_init ();
  gtk_main ();
  
  return 0;
}
