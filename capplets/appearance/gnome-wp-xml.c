/*
 *  Authors: Rodney Dawes <dobey@ximian.com>
 *
 *  Copyright 2003-2006 Novell, Inc. (www.novell.com)
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

#include "appearance.h"
#include "gnome-wp-item.h"
#include "gnome-wp-utils.h"
#include <string.h>
#include <libxml/parser.h>

static gboolean gnome_wp_xml_get_bool (const xmlNode * parent,
				       const gchar * prop_name) {
  xmlChar * prop;
  gboolean ret_val = FALSE;

  g_return_val_if_fail (parent != NULL, FALSE);
  g_return_val_if_fail (prop_name != NULL, FALSE);

  prop = xmlGetProp ((xmlNode *) parent, (xmlChar*)prop_name);
  if (prop != NULL) {
    if (!g_strcasecmp ((gchar *)prop, "true") || !g_strcasecmp ((gchar *)prop, "1")) {
      ret_val = TRUE;
    } else {
      ret_val = FALSE;
    }
    g_free (prop);
  }

  return ret_val;
}

static void gnome_wp_xml_set_bool (const xmlNode * parent,
				   const xmlChar * prop_name, gboolean value) {
  g_return_if_fail (parent != NULL);
  g_return_if_fail (prop_name != NULL);

  if (value) {
    xmlSetProp ((xmlNode *) parent, prop_name, (xmlChar *)"true");
  } else {
    xmlSetProp ((xmlNode *) parent, prop_name, (xmlChar *)"false");
  }
}

static void gnome_wp_load_legacy (AppearanceData *data) {
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

	item = g_hash_table_lookup (data->wp_hash, foo);
	if (item != NULL) {
	  continue;
	}

	if (!g_file_test (foo, G_FILE_TEST_EXISTS)) {
	  continue;
	}

	item = gnome_wp_item_new (foo, data->wp_hash, data->thumb_factory);
	if (item != NULL && item->fileinfo == NULL) {
	  gnome_wp_item_free (item);
	}
      }
      fclose (fp);
      g_free (foo);
    }
  }

  g_free (filename);
}

static void gnome_wp_xml_load_xml (AppearanceData *data,
				   const gchar * filename) {
  xmlDoc * wplist;
  xmlNode * root, * list, * wpa;
  xmlChar * nodelang;
  const gchar * const * syslangs;
  GdkColor color1, color2;
  GnomeWPItem * item;
  gint i;

#if GLIB_CHECK_VERSION (2, 6, 0)
  syslangs = g_get_language_names ();
#endif

  wplist = xmlParseFile (filename);

  if (!wplist)
    return;

  root = xmlDocGetRootElement (wplist);

  for (list = root->children; list != NULL; list = list->next) {
    if (!strcmp ((gchar *)list->name, "wallpaper")) {
      GnomeWPItem * wp;

      wp = g_new0 (GnomeWPItem, 1);

      wp->deleted = gnome_wp_xml_get_bool (list, "deleted");

      for (wpa = list->children; wpa != NULL; wpa = wpa->next) {
	if (!strcmp ((gchar *)wpa->name, "filename")) {
	  if (wpa->last != NULL && wpa->last->content != NULL) {
	    const char * none = "(none)";
	    gchar *content = g_strstrip ((gchar *)wpa->last->content);

	    if (!strncmp (content, none, strlen (none)))
	      wp->filename = g_strdup (content);
	    else if (g_utf8_validate (content, -1, NULL) &&
		     g_file_test (content, G_FILE_TEST_EXISTS))
	      wp->filename = g_strdup (content);
	    else
	      wp->filename = g_filename_from_utf8 (content, -1, NULL, NULL, NULL);
	  } else {
	    break;
	  }
	} else if (!strcmp ((gchar *)wpa->name, "name")) {
	  if (wpa->last != NULL && wpa->last->content != NULL) {
	    nodelang = xmlNodeGetLang (wpa->last);

	    if (wp->name == NULL && nodelang == NULL) {
	       wp->name = g_strdup (g_strstrip ((gchar *)wpa->last->content));
            } else {
	       for (i = 0; syslangs[i] != NULL; i++) {
	         if (!strcmp (syslangs[i], (gchar *)nodelang)) {
	           g_free (wp->name);
	           wp->name = g_strdup (g_strstrip ((gchar *)wpa->last->content));
	           break;
	         }
	       }
	    }

	    xmlFree (nodelang);
	  } else {
	    break;
	  }
	} else if (!strcmp ((gchar *)wpa->name, "imguri")) {
	  if (wpa->last != NULL) {
	    wp->imguri = g_strdup (g_strstrip ((gchar *)wpa->last->content));
	  }
	} else if (!strcmp ((gchar *)wpa->name, "options")) {
	  if (wpa->last != NULL) {
	    wp->options = g_strdup (g_strstrip ((gchar *)wpa->last->content));
	  } else {
	    wp->options = gconf_client_get_string (data->client,
						   WP_OPTIONS_KEY, NULL);
	  }
	} else if (!strcmp ((gchar *)wpa->name, "shade_type")) {
	  if (wpa->last != NULL) {
	    wp->shade_type = g_strdup (g_strstrip ((gchar *)wpa->last->content));
	  }
	} else if (!strcmp ((gchar *)wpa->name, "pcolor")) {
	  if (wpa->last != NULL) {
	    wp->pri_color = g_strdup (g_strstrip ((gchar *)wpa->last->content));
	  }
	} else if (!strcmp ((gchar *)wpa->name, "scolor")) {
	  if (wpa->last != NULL) {
	    wp->sec_color = g_strdup (g_strstrip ((gchar *)wpa->last->content));
	  }
	} else if (!strcmp ((gchar *)wpa->name, "text")) {
	  /* Do nothing here, libxml2 is being weird */
	} else {
	  g_warning ("Unknown Tag: %s\n", wpa->name);
	}
      }

      /* Make sure we don't already have this one and that filename exists */
      if (wp->filename != NULL) {
	item = g_hash_table_lookup (data->wp_hash, wp->filename);

	if (item != NULL) {
	  gnome_wp_item_free (wp);
	  continue;
	}
      } else {
	gnome_wp_item_free (wp);
	continue;
      }

      /* Verify the colors and alloc some GdkColors here */
      if (wp->shade_type == NULL) {
	wp->shade_type = gconf_client_get_string (data->client,
						  WP_SHADING_KEY, NULL);
      }
      if (wp->pri_color == NULL) {
	wp->pri_color = gconf_client_get_string (data->client,
						 WP_PCOLOR_KEY, NULL);
      }
      if (wp->sec_color == NULL) {
	wp->sec_color = gconf_client_get_string (data->client,
						 WP_SCOLOR_KEY, NULL);
      }
      gdk_color_parse (wp->pri_color, &color1);
      gdk_color_parse (wp->sec_color, &color2);

      wp->pcolor = gdk_color_copy (&color1);
      wp->scolor = gdk_color_copy (&color2);

      if ((wp->filename != NULL &&
	   g_file_test (wp->filename, G_FILE_TEST_EXISTS)) ||
	  !strcmp (wp->filename, "(none)")) {
	wp->fileinfo = gnome_wp_info_new (wp->filename, data->thumb_factory);

	if (wp->name == NULL || !strcmp (wp->filename, "(none)")) {
	  g_free (wp->name);
	  wp->name = g_strdup (wp->fileinfo->name);
	}

	gnome_wp_item_update_description (wp);
	g_hash_table_insert (data->wp_hash, wp->filename, wp);
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
				   AppearanceData *data) {
  gchar * filename;

  switch (event_type) {
  case GNOME_VFS_MONITOR_EVENT_CHANGED:
  case GNOME_VFS_MONITOR_EVENT_CREATED:
    filename = gnome_vfs_get_local_path_from_uri (info_uri);
    gnome_wp_xml_load_xml (data, filename);
    g_free (filename);
  default:
    break;
  }
}

void gnome_wp_xml_load_list (AppearanceData *data) {
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
    gnome_wp_xml_load_xml (data, wpdbfile);
  } else {
    g_free (wpdbfile);
    wpdbfile = g_build_filename (g_get_home_dir (),
				 ".gnome2",
				 "wp-list.xml",
				 NULL);
    if (g_file_test (wpdbfile, G_FILE_TEST_EXISTS)) {
      gnome_wp_xml_load_xml (data, wpdbfile);
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
    if (g_file_test (datadir, G_FILE_TEST_IS_DIR)) {
      gnome_vfs_directory_list_load (&list, datadir,
				     GNOME_VFS_FILE_INFO_DEFAULT |
				     GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

      for (l = list; l != NULL; l = l->next) {
	GnomeVFSFileInfo * info = l->data;

	if (strcmp (".", info->name) != 0 && strcmp ("..", info->name) != 0) {
	  gchar * filename;

	  filename = g_build_filename (datadir, info->name, NULL);
	  gnome_wp_xml_load_xml (data, filename);
	  g_free (filename);
	}
      }
      gnome_vfs_file_info_list_free (list);

      gnome_vfs_monitor_add (&handle, datadir, GNOME_VFS_MONITOR_DIRECTORY,
			     (GnomeVFSMonitorCallback) gnome_wp_file_changed,
			     data);
    }
    g_free (datadir);
  }
  g_strfreev (xdgdirs);
  g_free (xdgdirslist);

  if (g_file_test (WALLPAPER_DATADIR, G_FILE_TEST_IS_DIR)) {
    gnome_vfs_directory_list_load (&list, WALLPAPER_DATADIR,
				   GNOME_VFS_FILE_INFO_DEFAULT |
				   GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

    for (l = list; l != NULL; l = l->next) {
      GnomeVFSFileInfo * info = l->data;

      if (strcmp (".", info->name) != 0 && strcmp ("..", info->name) != 0) {
	gchar * filename;

	filename = g_build_filename (WALLPAPER_DATADIR, info->name, NULL);
	gnome_wp_xml_load_xml (data, filename);
	g_free (filename);
      }
    }
    gnome_vfs_file_info_list_free (list);

    gnome_vfs_monitor_add (&handle, WALLPAPER_DATADIR, GNOME_VFS_MONITOR_DIRECTORY,
			   (GnomeVFSMonitorCallback) gnome_wp_file_changed,
			   data);
  }

  gnome_wp_load_legacy (data);
}

static void gnome_wp_list_flatten (const gchar * key, GnomeWPItem * item,
				   GSList ** list) {
  g_return_if_fail (key != NULL);
  g_return_if_fail (item != NULL);

  *list = g_slist_prepend (*list, item);
}

void gnome_wp_xml_save_list (AppearanceData *data) {
  xmlDoc * wplist;
  xmlNode * root, * wallpaper, * item;
  GSList * list = NULL;
  gchar * wpfile;

  g_hash_table_foreach (data->wp_hash,
			       (GHFunc) gnome_wp_list_flatten, &list);
  g_hash_table_destroy (data->wp_hash);
  list = g_slist_reverse (list);

  wpfile = g_build_filename (g_get_home_dir (),
			     "/.gnome2",
			     "backgrounds.xml",
			     NULL);

  xmlKeepBlanksDefault (0);

  wplist = xmlNewDoc ((xmlChar *)"1.0");
  xmlCreateIntSubset (wplist, (xmlChar *)"wallpapers", NULL, (xmlChar *)"gnome-wp-list.dtd");
  root = xmlNewNode (NULL, (xmlChar *)"wallpapers");
  xmlDocSetRootElement (wplist, root);

  while (list != NULL) {
    GnomeWPItem * wpitem = list->data;
    const char * none = "(none)";
    gchar * filename;

    if (!strncmp (wpitem->filename, none, strlen (none)) ||
	(g_utf8_validate (wpitem->filename, -1, NULL) &&
	 g_file_test (wpitem->filename, G_FILE_TEST_EXISTS)))
      filename = g_strdup (wpitem->filename);
    else
      filename = g_filename_to_utf8 (wpitem->filename, -1, NULL, NULL, NULL);

    wallpaper = xmlNewChild (root, NULL, (xmlChar *)"wallpaper", NULL);
    gnome_wp_xml_set_bool (wallpaper, (xmlChar *)"deleted", wpitem->deleted);
    item = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"name", (xmlChar *)wpitem->name);
    item = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"filename", (xmlChar *)filename);
    item = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"options", (xmlChar *)wpitem->options);
    item = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"shade_type", (xmlChar *)wpitem->shade_type);
    item = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"pcolor", (xmlChar *)wpitem->pri_color);
    item = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"scolor", (xmlChar *)wpitem->sec_color);
    g_free (filename);

    list = g_slist_remove (list, wpitem);
    gnome_wp_item_free (wpitem);
  }
  xmlSaveFormatFile (wpfile, wplist, 1);
  xmlFreeDoc (wplist);
  g_free (wpfile);
}
