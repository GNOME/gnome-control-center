/*
 *  Authors: Rodney Dawes <dobey@ximian.com>
 *
 *  Copyright 2003-2004 Novell, Inc. (www.novell.com)
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

static void gnome_wp_capplet_scroll_to_item (GnomeWPCapplet * capplet,
					     GnomeWPItem * item) {
  GtkTreePath * path;

  g_return_if_fail (capplet != NULL);
  g_return_if_fail (item != NULL);

  path = gtk_tree_row_reference_get_path (item->rowref);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (capplet->treeview), path,
			    NULL, FALSE);
  gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (capplet->treeview),
				path, NULL, TRUE, 0.5, 0.0);
  gtk_tree_path_free (path);
}

static GnomeWPItem * gnome_wp_add_image (GnomeWPCapplet * capplet,
					 const gchar * filename) {
  GnomeWPItem * item;

  item = g_hash_table_lookup (capplet->wphash, filename);
  if (item != NULL) {
    if (item->deleted) {
      item->deleted = FALSE;
      wp_props_load_wallpaper (item->filename, item, capplet);
    }
  } else {
    item = gnome_wp_item_new (filename, capplet->wphash, capplet->thumbs);
    if (item != NULL) {
      wp_props_load_wallpaper (item->filename, item, capplet);
    }
  }

  return item;
}

static void gnome_wp_add_images (GnomeWPCapplet * capplet,
				 GSList * images) {
  GdkCursor * cursor;
  GnomeWPItem * item;

  item = NULL;
  cursor = gdk_cursor_new_for_display (gdk_display_get_default (),
				       GDK_WATCH);
  gdk_window_set_cursor (capplet->window->window, cursor);
  gdk_cursor_unref (cursor);

  if (images != NULL) {
    for (; images != NULL; images = images->next) {
      item = gnome_wp_add_image (capplet, images->data);
    }
  }

  gdk_window_set_cursor (capplet->window->window, NULL);

  if (item != NULL) {
    gnome_wp_capplet_scroll_to_item (capplet, item);
  }
}

static void gnome_wp_file_open_dialog (GtkWidget * widget,
				       GnomeWPCapplet * capplet) {
  GSList * files;

  switch (gtk_dialog_run (GTK_DIALOG (capplet->filesel))) {
  case GTK_RESPONSE_OK:
    files = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (capplet->filesel));
    gnome_wp_add_images (capplet, files);
    g_slist_free (files);
  case GTK_RESPONSE_CANCEL:
  default:
    gtk_widget_hide (capplet->filesel);
    break;
  }
}

static void bg_properties_dragged_image (GtkWidget * widget,
					 GdkDragContext * context,
					 gint x, gint y,
					 GtkSelectionData * selection_data,
					 guint info, guint time,
					 GnomeWPCapplet * capplet) {

  if (info == TARGET_URI_LIST || info == TARGET_BGIMAGE) {
    GList * uris;
    GSList * realuris = NULL;

    uris = gnome_vfs_uri_list_parse ((gchar *) selection_data->data);

    if (uris != NULL && uris->data != NULL) {
      GdkCursor * cursor;

      cursor = gdk_cursor_new_for_display (gdk_display_get_default (),
					   GDK_WATCH);
      gdk_window_set_cursor (capplet->window->window, cursor);
      gdk_cursor_unref (cursor);

      for (; uris != NULL; uris = uris->next) {
	realuris = g_slist_append (realuris,
				   g_strdup (gnome_vfs_uri_get_path (uris->data)));
      }
      gnome_wp_add_images (capplet, realuris);
      gdk_window_set_cursor (capplet->window->window, NULL);
    }
    gnome_vfs_uri_list_free (uris);
    g_slist_free (realuris);
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
  gnome_wp_item_update_description (item);

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
  GConfChangeSet * cs;
  gchar * wpfile;
  GdkPixbuf * pixbuf;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (capplet->treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_tree_model_get (model, &iter, 2, &wpfile, -1);

    item = g_hash_table_lookup (capplet->wphash, wpfile);

    cs = gconf_change_set_new ();

    if (!strcmp (item->filename, "(none)")) {
      gconf_change_set_set_string (cs, WP_OPTIONS_KEY, "none");
      gtk_widget_set_sensitive (capplet->wp_opts, FALSE);
      gtk_widget_set_sensitive (capplet->rm_button, FALSE);
    } else {
      gtk_widget_set_sensitive (capplet->wp_opts, TRUE);
      gtk_widget_set_sensitive (capplet->rm_button, TRUE);
      gconf_change_set_set_string (cs, WP_FILE_KEY, item->filename);
      gconf_change_set_set_string (cs, WP_OPTIONS_KEY, item->options);
      gnome_wp_option_menu_set (capplet, item->options, FALSE);
    }

    gconf_change_set_set_string (cs, WP_SHADING_KEY, item->shade_type);
    gnome_wp_option_menu_set (capplet, item->shade_type, TRUE);

    gconf_change_set_set_string (cs, WP_PCOLOR_KEY, item->pri_color);
    gconf_change_set_set_string (cs, WP_SCOLOR_KEY, item->sec_color);

    gconf_client_commit_change_set (capplet->client, cs, TRUE, NULL);

    gconf_change_set_unref (cs);

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
			"user-guide.xml", "goscustdesk-7");
    break;
  case GTK_RESPONSE_DELETE_EVENT:
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
  GdkPixbuf * pixbuf;
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
  pixbuf = gnome_wp_item_get_thumbnail (item, capplet->thumbs);
  gtk_list_store_set (GTK_LIST_STORE (capplet->model), &iter,
		      0, pixbuf,
		      -1);
  g_object_unref (pixbuf);
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
  GtkTreePath * first;
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

    g_free (wpfile);
  }
  first = gtk_tree_path_new_first ();
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (capplet->treeview),
			    first, NULL, FALSE);
  gtk_tree_path_free (first);
}

static gboolean gnome_wp_load_stuffs (void * data) {
  GnomeWPCapplet * capplet = (GnomeWPCapplet *) data;
  gchar * imagepath, * style;
  GnomeWPItem * item;

  style = gconf_client_get_string (capplet->client,
				   WP_OPTIONS_KEY,
				   NULL);

  gnome_wp_xml_load_list (capplet);
  g_hash_table_foreach (capplet->wphash, (GHFunc) wp_props_load_wallpaper,
			capplet);

  gdk_window_set_cursor (capplet->window->window, NULL);
  
  imagepath = gconf_client_get_string (capplet->client,
				       WP_FILE_KEY,
				       NULL);

  item = g_hash_table_lookup (capplet->wphash, imagepath);
  if (item != NULL && strcmp (style, "none") != 0) {
    if (item->deleted == TRUE) {
      item->deleted = FALSE;
      wp_props_load_wallpaper (item->filename, item, capplet);
    }

    gnome_wp_capplet_scroll_to_item (capplet, item);

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
  } else if (strcmp (style, "none") != 0) {
    item = gnome_wp_item_new (imagepath, capplet->wphash, capplet->thumbs);
    if (item != NULL) {
      wp_props_load_wallpaper (item->filename, item, capplet);

      gnome_wp_capplet_scroll_to_item (capplet, item);

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
    }
  }

  item = g_hash_table_lookup (capplet->wphash, "(none)");
  if (item == NULL) {
    item = gnome_wp_item_new ("(none)", capplet->wphash, capplet->thumbs);
    if (item != NULL) {
      wp_props_load_wallpaper (item->filename, item, capplet);
    }
  } else {
    if (item->deleted == TRUE) {
      item->deleted = FALSE;
      wp_props_load_wallpaper (item->filename, item, capplet);
    }

    if (!strcmp (style, "none")) {
      gnome_wp_capplet_scroll_to_item (capplet, item);
    }
  }
  g_free (imagepath);
  g_free (style);

  return FALSE;
}

static gint gnome_wp_list_sort (GtkTreeModel * model,
				GtkTreeIter * a, GtkTreeIter * b,
				GnomeWPCapplet * capplet) {
  gchar * foo, * bar;
  gchar * desca, * descb;
  gint retval;

  gtk_tree_model_get (model, a, 1, &desca, 2, &foo, -1);
  gtk_tree_model_get (model, b, 1, &descb, 2, &bar, -1);

  if (!strcmp (foo, "(none)")) {
    retval =  -1;
  } else if (!strcmp (bar, "(none)")) {
    retval =  1;
  } else {
    retval = g_utf8_collate (desca, descb);
  }

  g_free (desca);
  g_free (descb);
  g_free (foo);
  g_free (bar);

  return retval;
}

static void gnome_wp_file_changed (GConfClient * client, guint id,
				   GConfEntry * entry,
				   GnomeWPCapplet * capplet) {
  GtkTreeSelection * selection;
  GtkTreeModel * model;
  GtkTreeIter iter;
  GnomeWPItem * item;
  gchar * wpfile, * selected;

  wpfile = g_strdup (gconf_value_get_string (entry->value));
  item = g_hash_table_lookup (capplet->wphash, wpfile);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (capplet->treeview));
  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_tree_model_get (model, &iter, 2, &selected, -1);
    if (strcmp (selected, wpfile) != 0) {
      if (item != NULL) {
	gnome_wp_capplet_scroll_to_item (capplet, item);
      } else {
	item = gnome_wp_item_new (wpfile, capplet->wphash, capplet->thumbs);
	if (item != NULL) {
	  wp_props_load_wallpaper (item->filename, item, capplet);
      
	  gnome_wp_capplet_scroll_to_item (capplet, item);
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
  GdkPixbuf * pixbuf;
  gchar * icofile;

  icofile = gnome_icon_theme_lookup_icon (capplet->theme,
					  "background-capplet",
					  48, NULL, NULL);
  if (icofile != NULL) {
    pixbuf = gdk_pixbuf_new_from_file (icofile, NULL);
    gtk_window_set_icon (GTK_WINDOW (capplet->window), NULL);
    gtk_window_set_default_icon_from_file (icofile, NULL);
    g_object_unref (pixbuf);
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

static GladeXML * gnome_wp_create_dialog (void) {
  GladeXML * new;
  gchar * gladefile;

  gladefile = g_build_filename (GNOMECC_DATA_DIR,
				"interfaces",
				"gnome-background-properties.glade",
				NULL);

  if (!g_file_test (gladefile, G_FILE_TEST_EXISTS)) {
    gladefile = g_build_filename (g_get_current_dir (),
				  "gnome-background-properties.glade",
				  NULL);
  }
  new = glade_xml_new (gladefile, NULL, NULL);
  g_free (gladefile);

  return new;
}

static void set_accessible_name (GtkWidget *widget, const gchar *name) {
  AtkObject *obj;
 
  obj = gtk_widget_get_accessible (widget);
  if (!GTK_IS_ACCESSIBLE (obj))
    return;
  if (name)
    atk_object_set_name (obj, name);
} 
  
static void wallpaper_properties_init (void) {
  GnomeWPCapplet * capplet;
  GladeXML * dialog;
  GtkWidget * menu, * label;
  GtkWidget * mbox, * mitem;
  GtkWidget * add_button;
  GtkCellRenderer * renderer;
  GtkTreeViewColumn * column;
  GtkTreeSelection * selection;
  GdkPixbuf * pixbuf;
  GdkCursor * cursor;
  gchar * icofile;

  gtk_rc_parse_string ("style \"wp-tree-defaults\" {\n"
		       "  GtkTreeView::horizontal-separator = 6\n"
		       "} widget_class \"*TreeView*\""
		       " style \"wp-tree-defaults\"\n\n"
		       "style \"wp-dialog-defaults\" {\n"
		       "  GtkDialog::action-area-border = 0\n"
		       "  GtkDialog::content-area-border = 0\n"
		       "} widget_class \"*GtkDialog*\""
		       " style \"wp-dialog-defaults\"");

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

  dialog = gnome_wp_create_dialog ();
  capplet->window = glade_xml_get_widget (dialog,"gnome_wp_properties");

  icofile = gnome_icon_theme_lookup_icon (capplet->theme,
					  "background-capplet",
					  48, NULL, NULL);
  if (icofile != NULL) {
    pixbuf = gdk_pixbuf_new_from_file (icofile, NULL);
    gtk_window_set_default_icon_from_file (icofile, NULL);
    gtk_window_set_icon (GTK_WINDOW (capplet->window), pixbuf);
    g_object_unref (pixbuf);
  }
  g_free (icofile);

  gtk_widget_realize (capplet->window);

  gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (capplet->window)->action_area), 12);

  /* Drag and Drop Support */
  gtk_drag_dest_unset (capplet->window);
  gtk_drag_dest_set (capplet->window, GTK_DEST_DEFAULT_ALL, drop_types,
		     sizeof (drop_types) / sizeof (drop_types[0]),
		     GDK_ACTION_COPY | GDK_ACTION_MOVE);
  g_signal_connect (G_OBJECT (capplet->window), "drag_data_received",
		    G_CALLBACK (bg_properties_dragged_image), capplet);

  capplet->treeview = glade_xml_get_widget (dialog,"wp_tree");

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

  capplet->wp_opts = glade_xml_get_widget (dialog,"style_menu");

  menu = gtk_menu_new ();
  mitem = gtk_menu_item_new ();
  set_accessible_name (mitem, _("Centered")); 
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
  set_accessible_name (mitem, _("Fill Screen"));
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
  set_accessible_name (mitem, _("Scaled"));
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
  set_accessible_name (mitem, _("Tiled"));
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

  add_button = glade_xml_get_widget (dialog,"add_button");
  capplet->rm_button = glade_xml_get_widget (dialog,"rem_button");

  g_signal_connect (G_OBJECT (add_button), "clicked",
		    G_CALLBACK (gnome_wp_file_open_dialog), capplet);
  g_signal_connect (G_OBJECT (capplet->rm_button), "clicked",
		    G_CALLBACK (gnome_wp_remove_wallpaper), capplet);

  capplet->color_opt = glade_xml_get_widget (dialog,"color_menu");

  menu = gtk_menu_new ();
  mitem = gtk_menu_item_new ();
  set_accessible_name (mitem, _("Solid Color"));
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
  set_accessible_name (mitem, _("Horizontal Gradient"));
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
  set_accessible_name (mitem, _("Vertical Gradient"));
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

  capplet->pc_picker = glade_xml_get_widget (dialog,"pcpicker");
  g_signal_connect (G_OBJECT (capplet->pc_picker), "color_set",
		    G_CALLBACK (gnome_wp_pcolor_changed), capplet);

  capplet->sc_picker = glade_xml_get_widget (dialog,"scpicker");
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

  /* Create the file chooser dialog stuff here */
  capplet->filesel = gtk_file_chooser_dialog_new (_("Add Wallpaper"),
						  GTK_WINDOW (capplet->window),
						  GTK_FILE_CHOOSER_ACTION_OPEN,
						  GTK_STOCK_CANCEL,
						  GTK_RESPONSE_CANCEL,
						  GTK_STOCK_OPEN,
						  GTK_RESPONSE_OK,
						  NULL);
  gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (capplet->filesel),
					TRUE);
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
