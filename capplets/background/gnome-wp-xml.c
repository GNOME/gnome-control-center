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

static gboolean gnome_wp_xml_get_bool (const xmlNode * parent,
				       const gchar * prop_name) {
  gchar * prop;
  gboolean ret_val = FALSE;

  g_return_val_if_fail (parent != NULL, FALSE);
  g_return_val_if_fail (prop_name != NULL, FALSE);

  prop = xmlGetProp ((xmlNode *) parent, prop_name);
  if (prop != NULL) {
    if (!g_strcasecmp (prop, "true") || !g_strcasecmp (prop, "1")) {
      ret_val = TRUE;
    } else {
      ret_val = FALSE;
    }
    g_free (prop);
  }

  return ret_val;
}

static void gnome_wp_xml_set_bool (const xmlNode * parent,
				   const gchar * prop_name, gboolean value) {
  g_return_if_fail (parent != NULL);
  g_return_if_fail (prop_name != NULL);

  if (value) {
    xmlSetProp ((xmlNode *) parent, prop_name, "true");
  } else {
    xmlSetProp ((xmlNode *) parent, prop_name, "false");
  }
}

static void gnome_wp_load_legacy (GnomeWPCapplet * capplet) {
  FILE * fp;
  gchar * foo, * filename;

  filename = g_build_filename (g_get_home_dir (), ".gnome2",
			       "wallpapers.list", NULL);

  if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
    if ((fp = fopen (filename, "r")) != NULL) {
      foo = (gchar *) g_malloc (sizeof (gchar) * 4096);
      while (fgets (foo, 4096, fp)) {
	GnomeWPItem * item;

	if (foo[strlen (foo) - 1] == '\n') {
	  foo[strlen (foo) - 1] = '\0';
	}
	
	item = g_hash_table_lookup (capplet->wphash, foo);
	if (item != NULL) {
	  continue;
	}

	if (!g_file_test (foo, G_FILE_TEST_EXISTS)) {
	  continue;
	}

	item = gnome_wp_item_new (foo, capplet->wphash, capplet->thumbs);
	if (item != NULL && item->fileinfo == NULL) {
	  gnome_wp_item_free (item);
	}
      }
      fclose (fp);
      g_free (foo);
    }
  }
}

static void gnome_wp_xml_load_xml (GnomeWPCapplet * capplet,
				   const gchar * filename) {
  xmlDoc * wplist;
  xmlNode * root, * list, * wpa;
  GdkColor color1, color2;
  GnomeWPItem * item;

  wplist = xmlParseFile (filename);

  root = xmlDocGetRootElement (wplist);

  for (list = root->children; list != NULL; list = list->next) {
    if (!strcmp (list->name, "wallpaper")) {
      GnomeWPItem * wp;

      wp = g_new0 (GnomeWPItem, 1);

      wp->deleted = gnome_wp_xml_get_bool (list, "deleted");

      for (wpa = list->children; wpa != NULL; wpa = wpa->next) {
	if (!strcmp (wpa->name, "filename")) {
	  if (wpa->last != NULL) {
	    wp->filename = g_strdup (g_strstrip (wpa->last->content));
	  } else {
	    break;
	  }
	} else if (!strcmp (wpa->name, "name")) {
	  if (wpa->last != NULL) {
	    wp->name = g_strdup (g_strstrip (wpa->last->content));
	  }
	} else if (!strcmp (wpa->name, "imguri")) {
	  if (wpa->last != NULL) {
	    wp->imguri = g_strdup (g_strstrip (wpa->last->content));
	  }
	} else if (!strcmp (wpa->name, "options")) {
	  if (wpa->last != NULL) {
	    wp->options = g_strdup (g_strstrip (wpa->last->content));
	  } else {
	    wp->options = gconf_client_get_string (capplet->client,
						   WP_OPTIONS_KEY, NULL);
	  }
	} else if (!strcmp (wpa->name, "shade_type")) {
	  if (wpa->last != NULL) {
	    wp->shade_type = g_strdup (g_strstrip (wpa->last->content));
	  }
	} else if (!strcmp (wpa->name, "pcolor")) {
	  if (wpa->last != NULL) {
	    wp->pri_color = g_strdup (g_strstrip (wpa->last->content));
	  }
	} else if (!strcmp (wpa->name, "scolor")) {
	  if (wpa->last != NULL) {
	    wp->sec_color = g_strdup (g_strstrip (wpa->last->content));
	  }
	} else if (!strcmp (wpa->name, "text")) {
	  /* Do nothing here, libxml2 is being weird */
	} else {
	  g_warning ("Unknown Tag: %s\n", wpa->name);
	}
      }

      /* Make sure we don't already have this one */
      item = g_hash_table_lookup (capplet->wphash, wp->filename);

      if (item != NULL) {
	gnome_wp_item_free (wp);
	continue;
      }

      /* Verify the colors and alloc some GdkColors here */
      if (wp->shade_type == NULL) {
	wp->shade_type = gconf_client_get_string (capplet->client,
						  WP_SHADING_KEY, NULL);
      }
      if (wp->pri_color == NULL) {
	wp->pri_color = gconf_client_get_string (capplet->client,
						 WP_PCOLOR_KEY, NULL);
      }
      if (wp->sec_color == NULL) {
	wp->sec_color = gconf_client_get_string (capplet->client,
						 WP_SCOLOR_KEY, NULL);
      }
      gdk_color_parse (wp->pri_color, &color1);
      gdk_color_parse (wp->sec_color, &color2);

      wp->pcolor = gdk_color_copy (&color1);
      wp->scolor = gdk_color_copy (&color2);

      if ((wp->filename != NULL &&
	   g_file_test (wp->filename, G_FILE_TEST_EXISTS)) ||
	  !strcmp (wp->filename, "(none)")) {
	wp->fileinfo = gnome_wp_info_new (wp->filename, capplet->thumbs);

	if (wp->name == NULL || !strcmp (wp->filename, "(none)")) {
	  wp->name = g_strdup (wp->fileinfo->name);
	}

	gnome_wp_item_update_description (wp);
	g_hash_table_insert (capplet->wphash, g_strdup (wp->filename), wp);
      } else {
	gnome_wp_item_free (wp);
      }
    }
  }
  xmlFreeDoc (wplist);
}

static void gnome_wp_file_changed (GnomeVFSMonitorHandle * handle,
				   const gchar * monitor_uri,
				   const gchar * info_uri,
				   GnomeVFSMonitorEventType event_type,
				   GnomeWPCapplet * capplet) {
  gchar * filename;

  switch (event_type) {
  case GNOME_VFS_MONITOR_EVENT_CHANGED:
  case GNOME_VFS_MONITOR_EVENT_CREATED:
    filename = gnome_vfs_get_local_path_from_uri (info_uri);
    gnome_wp_xml_load_xml (capplet, filename);
    g_free (filename);
  default:
    break;
  }
}

void gnome_wp_xml_load_list (GnomeWPCapplet * capplet) {
  GnomeVFSMonitorHandle * handle;
  GList * list, * l;
  gchar * wpdbfile, * xdgdirslist;
  gchar ** xdgdirs;
  gint i;

  wpdbfile = g_build_filename (g_get_home_dir (),
			       ".gnome2",
			       "backgrounds.xml",
			       NULL);

  if (g_file_test (wpdbfile, G_FILE_TEST_EXISTS)) {
    gnome_wp_xml_load_xml (capplet, wpdbfile);
  } else {
    wpdbfile = g_build_filename (g_get_home_dir (),
				 ".gnome2",
				 "wp-list.xml",
				 NULL);
    if (g_file_test (wpdbfile, G_FILE_TEST_EXISTS)) {
      gnome_wp_xml_load_xml (capplet, wpdbfile);
    }
  }
  g_free (wpdbfile);

  xdgdirslist = g_strdup (g_getenv ("XDG_DATA_DIRS"));
  if (xdgdirslist == NULL || strlen (xdgdirslist) == 0)
    xdgdirslist = g_strdup ("/usr/local/share:/usr/share");

  xdgdirs = g_strsplit (xdgdirslist, ":", -1);
  for (i = 0; xdgdirs && xdgdirs[i]; i++) {
    gchar * datadir;

    datadir = g_build_filename (xdgdirs[i], "gnome-background-properties",
				NULL);
    if (g_file_test (datadir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
      gnome_vfs_directory_list_load (&list, datadir,
				     GNOME_VFS_FILE_INFO_DEFAULT |
				     GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

      for (l = list; l != NULL; l = l->next) {
	GnomeVFSFileInfo * info = l->data;

	if (strcmp (".", info->name) != 0 && strcmp ("..", info->name) != 0) {
	  gchar * filename;

	  filename = g_build_filename (datadir, info->name, NULL);
	  gnome_wp_xml_load_xml (capplet, filename);
	  g_free (filename);
	}
      }
      g_list_free (list);

      gnome_vfs_monitor_add (&handle, datadir, GNOME_VFS_MONITOR_DIRECTORY,
			     (GnomeVFSMonitorCallback) gnome_wp_file_changed,
			     capplet);
    }
    g_free (datadir);
  }
  g_strfreev (xdgdirs);
  g_free (xdgdirslist);

  if (g_file_test (WALLPAPER_DATADIR, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
    gnome_vfs_directory_list_load (&list, WALLPAPER_DATADIR,
				   GNOME_VFS_FILE_INFO_DEFAULT |
				   GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

    for (l = list; l != NULL; l = l->next) {
      GnomeVFSFileInfo * info = l->data;

      if (strcmp (".", info->name) != 0 && strcmp ("..", info->name) != 0) {
	gchar * filename;

	filename = g_build_filename (WALLPAPER_DATADIR, info->name, NULL);
	gnome_wp_xml_load_xml (capplet, filename);
	g_free (filename);
      }
    }
    g_list_free (list);

    gnome_vfs_monitor_add (&handle, WALLPAPER_DATADIR, GNOME_VFS_MONITOR_DIRECTORY,
			   (GnomeVFSMonitorCallback) gnome_wp_file_changed,
			   capplet);
  }

  gnome_wp_load_legacy (capplet);
}

static void gnome_wp_list_flatten (const gchar * key, GnomeWPItem * item,
				   GList ** list) {
  g_return_if_fail (key != NULL);
  g_return_if_fail (item != NULL);

  *list = g_list_append (*list, item);
}

void gnome_wp_xml_save_list (GnomeWPCapplet * capplet) {
  xmlDoc * wplist;
  xmlNode * root, * wallpaper, * item;
  GList * list = NULL, * wp = NULL;
  gchar * wpfile;

  g_hash_table_foreach (capplet->wphash,
			(GHFunc) gnome_wp_list_flatten, &list);

  wpfile = g_build_filename (g_get_home_dir (),
			     "/.gnome2",
			     "backgrounds.xml",
			     NULL);

  xmlKeepBlanksDefault (0);

  wplist = xmlNewDoc ("1.0");
  xmlCreateIntSubset (wplist, "wallpapers", NULL, "gnome-wp-list.dtd");
  root = xmlNewNode (NULL, "wallpapers");
  xmlDocSetRootElement (wplist, root);

  for (wp = list; wp != NULL; wp = wp->next) {
    GnomeWPItem * wpitem = wp->data;

    wallpaper = xmlNewChild (root, NULL, "wallpaper", NULL);
    gnome_wp_xml_set_bool (wallpaper, "deleted", wpitem->deleted);
    item = xmlNewTextChild (wallpaper, NULL, "name", wpitem->name);
    item = xmlNewTextChild (wallpaper, NULL, "filename", wpitem->filename);
    item = xmlNewTextChild (wallpaper, NULL, "options", wpitem->options);
    item = xmlNewTextChild (wallpaper, NULL, "shade_type", wpitem->shade_type);
    item = xmlNewTextChild (wallpaper, NULL, "pcolor", wpitem->pri_color);
    item = xmlNewTextChild (wallpaper, NULL, "scolor", wpitem->sec_color);
  }
  xmlSaveFormatFile (wpfile, wplist, 1);
  xmlFreeDoc (wplist);
  g_free (wpfile);
}

