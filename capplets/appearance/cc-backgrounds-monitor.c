/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Red Hat, Inc.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>

#include "cc-backgrounds-monitor.h"
#include "cc-background-item.h"

#define CC_BACKGROUNDS_MONITOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_BACKGROUNDS_MONITOR, CcBackgroundsMonitorPrivate))

struct CcBackgroundsMonitorPrivate
{
        GHashTable *item_hash;
};

enum {
        PROP_0,
};

enum {
        ITEM_ADDED,
        ITEM_REMOVED,
        LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0, };

static void     cc_backgrounds_monitor_class_init     (CcBackgroundsMonitorClass *klass);
static void     cc_backgrounds_monitor_init           (CcBackgroundsMonitor      *backgrounds_monitor);
static void     cc_backgrounds_monitor_finalize       (GObject             *object);

G_DEFINE_TYPE (CcBackgroundsMonitor, cc_backgrounds_monitor, G_TYPE_OBJECT)

#include <gio/gio.h>
#include <string.h>
#include <libxml/parser.h>

static gboolean
xml_get_bool (const xmlNode *parent,
              const char    *prop_name)
{
        xmlChar * prop;
        gboolean ret_val = FALSE;

        g_assert (parent != NULL);
        g_assert (prop_name != NULL);

        prop = xmlGetProp ((xmlNode *) parent, (xmlChar*)prop_name);
        if (prop == NULL) {
                goto done;
        }

        if (!g_ascii_strcasecmp ((char *)prop, "true")
            || !g_ascii_strcasecmp ((char *)prop, "1")) {
                ret_val = TRUE;
        } else {
                ret_val = FALSE;
        }
        g_free (prop);
 done:
        return ret_val;
}

static void
xml_set_bool (const xmlNode *parent,
              const xmlChar *prop_name,
              gboolean       value)
{
        g_assert (parent != NULL);
        g_assert (prop_name != NULL);

        if (value) {
                xmlSetProp ((xmlNode *) parent, prop_name, (xmlChar *)"true");
        } else {
                xmlSetProp ((xmlNode *) parent, prop_name, (xmlChar *)"false");
        }
}

static void
load_legacy (CcBackgroundsMonitor *monitor)
{
        FILE *fp;
        char *foo;
        char *filename;

        filename = g_build_filename (g_get_home_dir (),
                                     ".gnome2",
                                     "wallpapers.list",
                                     NULL);

        if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
                if ((fp = fopen (filename, "r")) != NULL) {
                        foo = (char *) g_malloc (sizeof (char) * 4096);
                        while (fgets (foo, 4096, fp)) {
                                CcBackgroundItem *item;

                                if (foo[strlen (foo) - 1] == '\n') {
                                        foo[strlen (foo) - 1] = '\0';
                                }

                                item = g_hash_table_lookup (monitor->priv->item_hash, foo);
                                if (item != NULL) {
                                        continue;
                                }

                                if (!g_file_test (foo, G_FILE_TEST_EXISTS)) {
                                        continue;
                                }

                                item = cc_background_item_new (foo);
                                if (cc_background_item_load (item)) {
                                        cc_backgrounds_monitor_add_item (monitor, item);
                                } else {
                                        g_object_unref (item);
                                }
                        }
                        fclose (fp);
                        g_free (foo);
                }
        }

        g_free (filename);
}

static void
load_xml (CcBackgroundsMonitor *monitor,
          const char           *xml_filename)
{
        xmlDoc             *wplist;
        xmlNode            *root;
        xmlNode            *list;
        xmlNode            *wpa;
        xmlChar            *nodelang;
        const char * const *syslangs;
        int                 i;

        //g_debug ("loading from: %s", xml_filename);

        wplist = xmlParseFile (xml_filename);

        if (wplist == NULL)
                return;

        syslangs = g_get_language_names ();

        root = xmlDocGetRootElement (wplist);

        for (list = root->children; list != NULL; list = list->next) {
                char             *filename;
                char             *name;
                char             *primary_color;
                char             *secondary_color;
                char             *options;
                char             *shade_type;
                gboolean          deleted;

                if (strcmp ((char *)list->name, "wallpaper") != 0) {
                        continue;
                }

                filename = NULL;
                name = NULL;
                options = NULL;
                shade_type = NULL;
                primary_color = NULL;
                secondary_color = NULL;

                deleted = xml_get_bool (list, "deleted");

                for (wpa = list->children; wpa != NULL; wpa = wpa->next) {
                        if (wpa->type == XML_COMMENT_NODE) {
                                continue;
                        }

                        if (strcmp ((char *)wpa->name, "filename") == 0) {
                                if (wpa->last != NULL
                                    && wpa->last->content != NULL) {
                                        char *content;

                                        content = g_strstrip ((char *)wpa->last->content);

                                        if (strcmp (content, "(none)") == 0)
                                                filename = g_strdup (content);
                                        else if (g_utf8_validate (content, -1, NULL) &&
                                                 g_file_test (content, G_FILE_TEST_EXISTS))
                                                filename = g_strdup (content);
                                        else
                                                filename = g_filename_from_utf8 (content, -1, NULL, NULL, NULL);
                                } else {
                                        break;
                                }
                        } else if (strcmp ((char *)wpa->name, "name") == 0) {
                                if (wpa->last != NULL && wpa->last->content != NULL) {
                                        nodelang = xmlNodeGetLang (wpa->last);

                                        if (name == NULL && nodelang == NULL) {
                                                name = g_strdup (g_strstrip ((char *)wpa->last->content));
                                        } else {
                                                for (i = 0; syslangs[i] != NULL; i++) {
                                                        if (strcmp (syslangs[i], (char *)nodelang) == 0) {
                                                                g_free (name);
                                                                name = g_strdup (g_strstrip ((char *)wpa->last->content));
                                                                break;
                                                        }
                                                }
                                        }

                                        xmlFree (nodelang);
                                } else {
                                        break;
                                }
                        } else if (strcmp ((char *)wpa->name, "options") == 0) {
                                if (wpa->last != NULL) {
                                        options = g_strdup (g_strstrip ((char *)wpa->last->content));
                                }
                        } else if (strcmp ((char *)wpa->name, "shade_type") == 0) {
                                if (wpa->last != NULL) {
                                        shade_type = g_strdup (g_strstrip ((char *)wpa->last->content));
                                }
                        } else if (strcmp ((char *)wpa->name, "pcolor") == 0) {
                                if (wpa->last != NULL) {
                                        primary_color = g_strdup (g_strstrip ((char *)wpa->last->content));
                                }
                        } else if (strcmp ((char *)wpa->name, "scolor") == 0) {
                                if (wpa->last != NULL) {
                                        secondary_color = g_strdup (g_strstrip ((char *)wpa->last->content));
                                }
                        } else if (strcmp ((char *)wpa->name, "text") == 0) {
                                /* Do nothing here, libxml2 is being weird */
                        } else {
                                g_warning ("Unknown Tag: %s", wpa->name);
                        }
                }

                /* Make sure we don't already have this one and that filename exists */
                if (filename != NULL
                    && g_hash_table_lookup (monitor->priv->item_hash, filename) == NULL) {
                        CcBackgroundItem *item;

                        item = cc_background_item_new (filename);
                        g_object_set (item,
                                      "name", name,
                                      "primary-color", primary_color,
                                      "secondary-color", secondary_color,
                                      "placement", options,
                                      "shading", shade_type,
                                      NULL);

                        if (cc_background_item_load (item)) {
                                if (cc_backgrounds_monitor_add_item (monitor, item)) {
                                        g_debug ("Added item %s", name);
                                }
                        }
                        g_object_unref (item);
                }

                g_free (filename);
                g_free (name);
                g_free (primary_color);
                g_free (secondary_color);
                g_free (options);
                g_free (shade_type);
        }
        xmlFreeDoc (wplist);
}

static void
file_changed (GFileMonitor         *file_monitor,
              GFile                *file,
              GFile                *other_file,
              GFileMonitorEvent     event_type,
              CcBackgroundsMonitor *monitor)
{
        char *filename;

        switch (event_type) {
        case G_FILE_MONITOR_EVENT_CHANGED:
        case G_FILE_MONITOR_EVENT_CREATED:
                filename = g_file_get_path (file);
                load_xml (monitor, filename);
                g_free (filename);
                break;
        default:
                break;
        }
}

static void
add_monitor (CcBackgroundsMonitor *monitor,
             GFile                *directory)
{
        GFileMonitor *file_monitor;
        GError       *error = NULL;

        file_monitor = g_file_monitor_directory (directory,
                                                 G_FILE_MONITOR_NONE,
                                                 NULL,
                                                 &error);
        if (error != NULL) {
                char *path;

                path = g_file_get_parse_name (directory);
                g_warning ("Unable to monitor directory %s: %s",
                           path,
                           error->message);
                g_error_free (error);
                g_free (path);
                return;
        }

        g_signal_connect (file_monitor,
                          "changed",
                          G_CALLBACK (file_changed),
                          monitor);
}

static void
load_from_dir (CcBackgroundsMonitor *monitor,
               const char           *path)
{
        GFile           *directory;
        GFileEnumerator *enumerator;
        GError          *error;
        GFileInfo       *info;

        if (!g_file_test (path, G_FILE_TEST_IS_DIR)) {
                return;
        }

        g_debug ("Loading from directory %s", path);
        directory = g_file_new_for_path (path);

        error = NULL;
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
                const char *filename;
                char       *fullpath;

                filename = g_file_info_get_name (info);
                fullpath = g_build_filename (path, filename, NULL);
                g_object_unref (info);

                load_xml (monitor, fullpath);
                g_free (fullpath);
        }
        g_file_enumerator_close (enumerator, NULL, NULL);

        add_monitor (monitor, directory);

        g_object_unref (directory);
}

static void
ensure_none (CcBackgroundsMonitor *monitor)
{
        CcBackgroundItem *item;

        item = g_hash_table_lookup (monitor->priv->item_hash, "(none)");
        if (item == NULL) {
                item = cc_background_item_new ("(none)");
                if (cc_background_item_load (item)) {
                        cc_backgrounds_monitor_add_item (monitor, item);
                } else {
                        g_object_unref (item);
                }
        } else {
                g_object_set (item, "is-deleted", FALSE, NULL);
        }
}

void
cc_backgrounds_monitor_load (CcBackgroundsMonitor *monitor)
{
        const char * const *system_data_dirs;
        char               *filename;
        int                 i;

        g_return_if_fail (monitor != NULL);

        filename = g_build_filename (g_get_home_dir (),
                                     ".gnome2",
                                     "backgrounds.xml",
                                     NULL);

        if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
                load_xml (monitor, filename);
        } else {
                g_free (filename);
                filename = g_build_filename (g_get_home_dir (),
                                             ".gnome2",
                                             "wp-list.xml",
                                             NULL);
                if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
                        load_xml (monitor, filename);
                }
        }
        g_free (filename);

        filename = g_build_filename (g_get_user_data_dir (),
                                     "gnome-background-properties",
                                     NULL);
        load_from_dir (monitor, filename);
        g_free (filename);

        system_data_dirs = g_get_system_data_dirs ();
        for (i = 0; system_data_dirs[i]; i++) {
                filename = g_build_filename (system_data_dirs[i],
                                             "gnome-background-properties",
                                             NULL);
                load_from_dir (monitor, filename);
                g_free (filename);
        }

        load_from_dir (monitor, WALLPAPER_DATADIR);

        load_legacy (monitor);

        /* We always want to have a (none) entry */
        ensure_none (monitor);
}

static void
list_flatten (const char       *key,
              CcBackgroundItem *item,
              GList           **list)
{
        g_return_if_fail (key != NULL);
        g_return_if_fail (item != NULL);

        *list = g_list_prepend (*list, item);
}

gboolean
cc_backgrounds_monitor_add_item (CcBackgroundsMonitor *monitor,
                                 CcBackgroundItem     *item)
{
        char    *uri;
        gboolean deleted;
        gboolean ret;

        ret = FALSE;

        g_return_val_if_fail (monitor != NULL, FALSE);
        g_return_val_if_fail (item != NULL, FALSE);

        uri = NULL;
        g_object_get (item,
                      "filename", &uri,
                      "is-deleted", &deleted,
                      NULL);
        if (g_hash_table_lookup (monitor->priv->item_hash, uri) == NULL) {
                g_debug ("Inserting %s", uri);
                g_hash_table_insert (monitor->priv->item_hash,
                                     g_strdup (uri),
                                     g_object_ref (item));
                ret = TRUE;
        } else if (!deleted) {
                g_debug ("Undeleting %s", uri);
                g_object_set (item, "is-deleted", FALSE, NULL);
                ret = TRUE;
        }

        if (ret)
                g_signal_emit (monitor, signals [ITEM_ADDED], 0, item);

        g_free (uri);

        return ret;
}

gboolean
cc_backgrounds_monitor_remove_item (CcBackgroundsMonitor *monitor,
                                    CcBackgroundItem     *item)
{
        char    *uri;
        gboolean deleted;
        gboolean ret;

        ret = FALSE;

        g_return_val_if_fail (monitor != NULL, FALSE);
        g_return_val_if_fail (item != NULL, FALSE);

        g_object_get (item,
                      "filename", &uri,
                      "is-deleted", &deleted,
                      NULL);
        if (g_hash_table_lookup (monitor->priv->item_hash, uri) != NULL
            && !deleted) {
                g_object_set (item, "is-deleted", TRUE, NULL);
                g_signal_emit (monitor, signals [ITEM_REMOVED], 0, item);
                ret = TRUE;
        }
        g_free (uri);
        return ret;
}

GList *
cc_backgrounds_monitor_get_items (CcBackgroundsMonitor *monitor)
{
        GList *list;

        g_return_val_if_fail (monitor != NULL, NULL);

        list = NULL;
        g_hash_table_foreach (monitor->priv->item_hash,
                              (GHFunc) list_flatten,
                              &list);
        list = g_list_reverse (list);
        return list;
}

void
cc_backgrounds_monitor_save (CcBackgroundsMonitor *monitor)
{
        xmlDoc  *wplist;
        xmlNode *root;
        xmlNode *wallpaper;
        xmlNode *node;
        GList   *list = NULL;
        char    *wpfile;

        g_return_if_fail (monitor != NULL);

        g_hash_table_foreach (monitor->priv->item_hash,
                              (GHFunc) list_flatten,
                              &list);
        list = g_list_reverse (list);

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
                CcBackgroundItem *item;
                char             *scale;
                char             *shade;
                char             *filename;
                char             *utf8_filename;
                char             *name;
                char             *pcolor;
                char             *scolor;
                gboolean          deleted;

                item = list->data;
                g_object_get (item,
                              "filename", &filename,
                              "name", &name,
                              "primary-color", &pcolor,
                              "secondary-color", &scolor,
                              "shading", &shade,
                              "placement", &scale,
                              "is-deleted", &deleted,
                              NULL);

                if (strcmp (filename, "(none)") == 0
                    || (g_utf8_validate (filename, -1, NULL)
                        && g_file_test (filename, G_FILE_TEST_EXISTS))) {
                        utf8_filename = g_strdup (filename);
                } else {
                        utf8_filename = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
                }

                wallpaper = xmlNewChild (root, NULL, (xmlChar *)"wallpaper", NULL);
                xml_set_bool (wallpaper, (xmlChar *)"deleted", deleted);
                node = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"name", (xmlChar *)name);
                node = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"filename", (xmlChar *)filename);
                node = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"options", (xmlChar *)scale);
                node = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"shade_type", (xmlChar *)shade);
                node = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"pcolor", (xmlChar *)pcolor);
                node = xmlNewTextChild (wallpaper, NULL, (xmlChar *)"scolor", (xmlChar *)scolor);

                g_free (pcolor);
                g_free (scolor);
                g_free (filename);
                g_free (utf8_filename);
                g_free (name);
                g_free (shade);
                g_free (scale);

                list = g_list_delete_link (list, list);
        }

        xmlSaveFormatFile (wpfile, wplist, 1);
        xmlFreeDoc (wplist);
        g_free (wpfile);
}

static GObject *
cc_backgrounds_monitor_constructor (GType                  type,
                                    guint                  n_construct_properties,
                                    GObjectConstructParam *construct_properties)
{
        CcBackgroundsMonitor      *backgrounds_monitor;

        backgrounds_monitor = CC_BACKGROUNDS_MONITOR (G_OBJECT_CLASS (cc_backgrounds_monitor_parent_class)->constructor (type,
                                                                                                                         n_construct_properties,
                                                                                                                         construct_properties));

        return G_OBJECT (backgrounds_monitor);
}

static void
cc_backgrounds_monitor_class_init (CcBackgroundsMonitorClass *klass)
{
        GObjectClass  *object_class = G_OBJECT_CLASS (klass);

        object_class->constructor = cc_backgrounds_monitor_constructor;
        object_class->finalize = cc_backgrounds_monitor_finalize;

        signals [ITEM_ADDED]
                = g_signal_new ("item-added",
                                G_TYPE_FROM_CLASS (object_class),
                                G_SIGNAL_RUN_LAST,
                                0,
                                NULL,
                                NULL,
                                g_cclosure_marshal_VOID__OBJECT,
                                G_TYPE_NONE,
                                1, CC_TYPE_BACKGROUND_ITEM);
        signals [ITEM_REMOVED]
                = g_signal_new ("item-removed",
                                G_TYPE_FROM_CLASS (object_class),
                                G_SIGNAL_RUN_LAST,
                                0,
                                NULL,
                                NULL,
                                g_cclosure_marshal_VOID__OBJECT,
                                G_TYPE_NONE,
                                1, CC_TYPE_BACKGROUND_ITEM);

        g_type_class_add_private (klass, sizeof (CcBackgroundsMonitorPrivate));
}

static void
cc_backgrounds_monitor_init (CcBackgroundsMonitor *monitor)
{
        monitor->priv = CC_BACKGROUNDS_MONITOR_GET_PRIVATE (monitor);

        monitor->priv->item_hash = g_hash_table_new_full (g_str_hash,
                                                          g_str_equal,
                                                          g_free,
                                                          g_object_unref);
}

static void
cc_backgrounds_monitor_finalize (GObject *object)
{
        CcBackgroundsMonitor *monitor;

        g_return_if_fail (object != NULL);
        g_return_if_fail (CC_IS_BACKGROUNDS_MONITOR (object));

        monitor = CC_BACKGROUNDS_MONITOR (object);

        g_return_if_fail (monitor->priv != NULL);

        g_hash_table_destroy (monitor->priv->item_hash);

        G_OBJECT_CLASS (cc_backgrounds_monitor_parent_class)->finalize (object);
}

CcBackgroundsMonitor *
cc_backgrounds_monitor_new (void)
{
        GObject *object;

        object = g_object_new (CC_TYPE_BACKGROUNDS_MONITOR, NULL);

        return CC_BACKGROUNDS_MONITOR (object);
}
