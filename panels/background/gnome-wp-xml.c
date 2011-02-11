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

#include <gio/gio.h>
#include <string.h>
#include <libxml/parser.h>
#include <libgnome-desktop/gnome-bg.h>
#include <gsettings-desktop-schemas/gdesktop-enums.h>

#include "gdesktop-enums-types.h"
#include "cc-background-item.h"
#include "gnome-wp-xml.h"

static gboolean gnome_wp_xml_get_bool (const xmlNode * parent,
				       const gchar * prop_name) {
  xmlChar * prop;
  gboolean ret_val = FALSE;

  g_return_val_if_fail (parent != NULL, FALSE);
  g_return_val_if_fail (prop_name != NULL, FALSE);

  prop = xmlGetProp ((xmlNode *) parent, (xmlChar*)prop_name);
  if (prop != NULL) {
    if (!g_ascii_strcasecmp ((gchar *)prop, "true") || !g_ascii_strcasecmp ((gchar *)prop, "1")) {
      ret_val = TRUE;
    } else {
      ret_val = FALSE;
    }
    g_free (prop);
  }

  return ret_val;
}

#if 0
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
#endif

static struct {
	int value;
	const char *string;
} lookups[] = {
	{ G_DESKTOP_BACKGROUND_SHADING_HORIZONTAL, "horizontal-gradient" },
	{ G_DESKTOP_BACKGROUND_SHADING_VERTICAL, "vertical-gradient" },
};

static int
enum_string_to_value (GType type,
		      const char *string)
{
	GEnumClass *eclass;
	GEnumValue *value;

	eclass = G_ENUM_CLASS (g_type_class_peek (type));
	value = g_enum_get_value_by_nick (eclass, string);

	/* Here's a bit of hand-made parsing, bad bad */
	if (value == NULL) {
		guint i;
		for (i = 0; i < G_N_ELEMENTS (lookups); i++) {
			if (g_str_equal (lookups[i].string, string))
				return lookups[i].value;
		}
		g_warning ("Unhandled value '%s' for enum '%s'",
			   string, G_FLAGS_CLASS_TYPE_NAME (eclass));
		g_assert_not_reached ();
	}

	return value->value;
}

#define NONE "(none)"
#define UNSET_FLAG(flag) G_STMT_START{ (flags&=~(flag)); }G_STMT_END
#define SET_FLAG(flag) G_STMT_START{ (flags|=flag); }G_STMT_END

static void gnome_wp_xml_load_xml (GnomeWpXml *data,
				   const gchar * filename) {
  xmlDoc * wplist;
  xmlNode * root, * list, * wpa;
  xmlChar * nodelang;
  const gchar * const * syslangs;
  gint i;
  char *fname;

  wplist = xmlParseFile (filename);

  if (!wplist)
    return;

  syslangs = g_get_language_names ();
  fname = NULL;

  root = xmlDocGetRootElement (wplist);

  for (list = root->children; list != NULL; list = list->next) {
    if (!strcmp ((gchar *)list->name, "wallpaper")) {
      CcBackgroundItem * item;
      CcBackgroundItemFlags flags = 0;

      item = cc_background_item_new (NULL);

      g_object_set (G_OBJECT (item),
		    "is-deleted", gnome_wp_xml_get_bool (list, "deleted"),
		    "source-xml", filename,
		    NULL);

      for (wpa = list->children; wpa != NULL; wpa = wpa->next) {
	if (wpa->type == XML_COMMENT_NODE) {
	  continue;
	} else if (!strcmp ((gchar *)wpa->name, "filename")) {
	  if (wpa->last != NULL && wpa->last->content != NULL) {
	    gchar *content = g_strstrip ((gchar *)wpa->last->content);

	    if (strcmp (content, NONE) == 0) {
	      fname = NULL;
	    } else if (g_utf8_validate (content, -1, NULL) &&
		     g_file_test (content, G_FILE_TEST_EXISTS)) {
	      fname = g_strdup (content);
	    } else {
	      fname = g_filename_from_utf8 (content, -1, NULL, NULL, NULL);
	    }
	    SET_FLAG(CC_BACKGROUND_ITEM_HAS_FNAME);
	    g_object_set (G_OBJECT (item), "filename", fname, NULL);
	  } else {
	    break;
	  }
	} else if (!strcmp ((gchar *)wpa->name, "name")) {
	  if (wpa->last != NULL && wpa->last->content != NULL) {
	    char *name;
	    nodelang = xmlNodeGetLang (wpa->last);

	    g_object_get (G_OBJECT (item), "name", &name, NULL);

	    if (name == NULL && nodelang == NULL) {
	       g_object_set (G_OBJECT (item), "name",
			     g_strstrip ((gchar *)wpa->last->content), NULL);
            } else {
	       for (i = 0; syslangs[i] != NULL; i++) {
	         if (!strcmp (syslangs[i], (gchar *)nodelang)) {
		   g_object_set (G_OBJECT (item), "name",
				 g_strstrip ((gchar *)wpa->last->content), NULL);
	           break;
	         }
	       }
	    }

	    g_free (name);
	    xmlFree (nodelang);
	  } else {
	    break;
	  }
	} else if (!strcmp ((gchar *)wpa->name, "options")) {
	  if (wpa->last != NULL) {
	    g_object_set (G_OBJECT (item), "placement",
			  enum_string_to_value (G_DESKTOP_TYPE_DESKTOP_BACKGROUND_STYLE,
						g_strstrip ((gchar *)wpa->last->content)), NULL);
	    SET_FLAG(CC_BACKGROUND_ITEM_HAS_PLACEMENT);
	  }
	} else if (!strcmp ((gchar *)wpa->name, "shade_type")) {
	  if (wpa->last != NULL) {
	    g_object_set (G_OBJECT (item), "shading",
			  enum_string_to_value (G_DESKTOP_TYPE_DESKTOP_BACKGROUND_SHADING,
						g_strstrip ((gchar *)wpa->last->content)), NULL);
	    SET_FLAG(CC_BACKGROUND_ITEM_HAS_SHADING);
	  }
	} else if (!strcmp ((gchar *)wpa->name, "pcolor")) {
	  if (wpa->last != NULL) {
	    g_object_set (G_OBJECT (item), "primary-color",
			  g_strstrip ((gchar *)wpa->last->content), NULL);
	    SET_FLAG(CC_BACKGROUND_ITEM_HAS_PCOLOR);
	  }
	} else if (!strcmp ((gchar *)wpa->name, "scolor")) {
	  if (wpa->last != NULL) {
	    g_object_set (G_OBJECT (item), "secondary-color",
			  g_strstrip ((gchar *)wpa->last->content), NULL);
	    SET_FLAG(CC_BACKGROUND_ITEM_HAS_SCOLOR);
	  }
	} else if (!strcmp ((gchar *)wpa->name, "text")) {
	  /* Do nothing here, libxml2 is being weird */
	} else {
	  g_warning ("Unknown Tag: %s", wpa->name);
	}
      }

      /* Make sure we don't already have this one and that filename exists */
      if (fname == NULL ||
	  g_hash_table_lookup (data->wp_hash, fname) != NULL) {

	g_object_unref (item);
	continue;
      }

      g_object_set (G_OBJECT (item), "flags", flags, NULL);

      if (fname != NULL) {
#if 0
        cc_background_item_ensure_gnome_bg (wp);
	cc_background_item_update_size (wp, NULL);
#endif
	g_hash_table_insert (data->wp_hash, g_strdup (filename), item);
      } else {
	g_object_unref (item);
        item = NULL;
      }
    }
  }
  g_free (fname);
  xmlFreeDoc (wplist);
}

static void gnome_wp_file_changed (GFileMonitor *monitor,
                                   GFile *file,
                                   GFile *other_file,
                                   GFileMonitorEvent event_type,
                                   GnomeWpXml *data) {
  gchar * filename;

  switch (event_type) {
  case G_FILE_MONITOR_EVENT_CHANGED:
  case G_FILE_MONITOR_EVENT_CREATED:
    filename = g_file_get_path (file);
    gnome_wp_xml_load_xml (data, filename);
    g_free (filename);
    break;
  default:
    break;
  }
}

static void gnome_wp_xml_add_monitor (GFile      *directory,
                                      GnomeWpXml *data) {
  GFileMonitor *monitor;
  GError *error = NULL;

  monitor = g_file_monitor_directory (directory,
                                      G_FILE_MONITOR_NONE,
                                      NULL,
                                      &error);
  if (error != NULL) {
    gchar *path;

    path = g_file_get_parse_name (directory);
    g_warning ("Unable to monitor directory %s: %s",
               path, error->message);
    g_error_free (error);
    g_free (path);
    return;
  }

  g_signal_connect (monitor, "changed",
                    G_CALLBACK (gnome_wp_file_changed),
                    data);
}

static void gnome_wp_xml_load_from_dir (const gchar *path,
                                        GnomeWpXml  *data) {
  GFile *directory;
  GFileEnumerator *enumerator;
  GError *error = NULL;
  GFileInfo *info;

  if (!g_file_test (path, G_FILE_TEST_IS_DIR)) {
    return;
  }

  directory = g_file_new_for_path (path);
  enumerator = g_file_enumerate_children (directory,
                                          G_FILE_ATTRIBUTE_STANDARD_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          NULL,
                                          &error);
  if (error != NULL) {
    g_warning ("Unable to check directory %s: %s", path, error->message);
    g_error_free (error);
    g_object_unref (directory);
    return;
  }

  while ((info = g_file_enumerator_next_file (enumerator, NULL, NULL))) {
    const gchar *filename;
    gchar *fullpath;

    filename = g_file_info_get_name (info);
    fullpath = g_build_filename (path, filename, NULL);
    g_object_unref (info);

    gnome_wp_xml_load_xml (data, fullpath);
    g_free (fullpath);
  }
  g_file_enumerator_close (enumerator, NULL, NULL);

  gnome_wp_xml_add_monitor (directory, data);

  g_object_unref (directory);
  g_object_unref (enumerator);
}

void gnome_wp_xml_load_list (GnomeWpXml *data) {
  const char * const *system_data_dirs;
  gchar * datadir;
  gint i;

  datadir = g_build_filename (g_get_user_data_dir (),
                              "gnome-background-properties",
                              NULL);
  gnome_wp_xml_load_from_dir (datadir, data);
  g_free (datadir);

  system_data_dirs = g_get_system_data_dirs ();
  for (i = 0; system_data_dirs[i]; i++) {
    datadir = g_build_filename (system_data_dirs[i],
                                "gnome-background-properties",
				NULL);
    gnome_wp_xml_load_from_dir (datadir, data);
    g_free (datadir);
  }
}

GnomeWpXml *
gnome_wp_xml_load_list_finish (GAsyncResult  *async_result)
{
	GSimpleAsyncResult *result = G_SIMPLE_ASYNC_RESULT (async_result);

	g_return_val_if_fail (G_IS_ASYNC_RESULT (async_result), NULL);
	g_warn_if_fail (g_simple_async_result_get_source_tag (result) == gnome_wp_xml_load_list_async);

	return g_simple_async_result_get_op_res_gpointer (result);
}

static void
load_list_thread (GSimpleAsyncResult *res,
		  GObject *object,
		  GCancellable *cancellable)
{
	GnomeWpXml *data;

	data = g_simple_async_result_get_op_res_gpointer (res);
	gnome_wp_xml_load_list (data);
}

void gnome_wp_xml_load_list_async (GnomeWpXml *data,
				   GCancellable *cancellable,
				   GAsyncReadyCallback callback,
				   gpointer user_data)
{
	GSimpleAsyncResult *result;

	g_return_if_fail (data != NULL);

	result = g_simple_async_result_new (NULL, callback, user_data, gnome_wp_xml_load_list_async);
	g_simple_async_result_set_op_res_gpointer (result, data, NULL);
	g_simple_async_result_run_in_thread (result, (GSimpleAsyncThreadFunc) load_list_thread, G_PRIORITY_LOW, cancellable);
	g_object_unref (result);
}

#if 0
static void gnome_wp_list_flatten (const gchar * key, CcBackgroundItem * item,
				   GSList ** list) {
  g_return_if_fail (key != NULL);
  g_return_if_fail (item != NULL);

  *list = g_slist_prepend (*list, item);
}
#endif
void gnome_wp_xml_save_list (GnomeWpXml *data) {
	//FIXME implement save or remove?
#if 0
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
    CcBackgroundItem * wpitem = list->data;
    const char * none = "(none)";
    gchar * filename;
    const gchar * scale, * shade;
    gchar * pcolor, * scolor;

    if (!strcmp (wpitem->filename, none) ||
	(g_utf8_validate (wpitem->filename, -1, NULL) &&
	 g_file_test (wpitem->filename, G_FILE_TEST_EXISTS)))
      filename = g_strdup (wpitem->filename);
    else
      filename = g_filename_to_utf8 (wpitem->filename, -1, NULL, NULL, NULL);

    pcolor = gdk_color_to_string (wpitem->pcolor);
    scolor = gdk_color_to_string (wpitem->scolor);
    scale = wp_item_option_to_string (wpitem->options);
    shade = wp_item_shading_to_string (wpitem->shade_type);

    wallpaper = xmlNewChild (root, NULL, (xmlChar *)"wallpaper", NULL);
    gnome_wp_xml_set_bool (wallpaper, (xmlChar *)"deleted", wpitem->deleted);
    item = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"name", (xmlChar *)wpitem->name);
    item = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"filename", (xmlChar *)filename);
    item = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"options", (xmlChar *)scale);
    item = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"shade_type", (xmlChar *)shade);
    item = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"pcolor", (xmlChar *)pcolor);
    item = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"scolor", (xmlChar *)scolor);
    g_free (pcolor);
    g_free (scolor);
    g_free (filename);

    list = g_slist_delete_link (list, list);
    g_object_unref (wpitem);
  }
  xmlSaveFormatFile (wpfile, wplist, 1);
  xmlFreeDoc (wplist);
  g_free (wpfile);
#endif
}

