/* -*- mode: c; style: linux -*- */

/* mime-types-model.c
 *
 * Copyright (C) 2002 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-mime-utils.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include "mime-types-model.h"

const gchar *categories[] = {
	"Documents", "Images", "Audio", "Video", "Internet Services", NULL
};

const gchar *url_descriptions[][2] = {
	{ "unknown", N_("Unknown service types") },
	{ "http",    N_("World wide web") },
	{ "ftp",     N_("File transfer protocol") },
	{ "info",    N_("Detailed documentation") },
	{ "man",     N_("Manual pages") },
	{ "mailto",  N_("Electronic mail transmission") },
	{ NULL,      NULL }
};

static gchar *
get_category_path_for_mime_type (const gchar *mime_type) 
{
	const gchar *path;

	path = gnome_vfs_mime_get_value (mime_type, "category");

	if (path != NULL)
		return g_strdup (path);
	else if (!strncmp (mime_type, "image", strlen ("image")))
		return "Images";
	else if (!strncmp (mime_type, "video", strlen ("video")))
		return "Video";
	else if (!strncmp (mime_type, "audio", strlen ("audio")))
		return "Audio";
	else
		return NULL;
}

static void
get_path_num_from_str (GtkTreeStore *model, GtkTreeIter *iter, const gchar *path_str, GString *path_num) 
{
	gchar       *first_component;
	gchar       *rest_components;
	GValue       value;
	GtkTreeIter  child_iter;
	int          i, n;

	if (path_str == NULL || *path_str == '\0')
		return;

	rest_components = strchr (path_str, '/');

	if (rest_components != NULL) {
		first_component = g_strndup (path_str, rest_components - path_str);
		rest_components++;
	} else {
		first_component = g_strdup (path_str);
	}

	gtk_tree_model_iter_children (GTK_TREE_MODEL (model), &child_iter, iter);
	n = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (model), iter);

	value.g_type = G_TYPE_INVALID;

	for (i = 0; i < n; i++) {
		gtk_tree_model_get_value (GTK_TREE_MODEL (model), &child_iter, DESCRIPTION_COLUMN, &value);

		if (!strcmp (first_component, g_value_get_string (&value))) {
			g_string_append_printf (path_num, ":%d", i);
			get_path_num_from_str (model, &child_iter, rest_components, path_num);
			g_free (first_component);
			return;
		}

		gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &child_iter);
		g_value_unset (&value);
	}

	gtk_tree_store_append (model, &child_iter, iter);
	gtk_tree_store_set (model, &child_iter, DESCRIPTION_COLUMN, first_component, -1);
	g_string_append_printf (path_num, ":%d", n);
	g_free (first_component);
}

static void
get_insertion_point (GtkTreeStore *model, const gchar *path_str, GtkTreeIter *iter) 
{
	GString *path_num;

	path_num = g_string_new ("");
	get_path_num_from_str (model, NULL, path_str, path_num);
	gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (model), iter, path_num->str + 1);
	g_string_free (path_num, TRUE);
}

static const gchar *
get_protocol_name (const gchar *key) 
{
	gchar *protocol_name;

	protocol_name = strrchr (key, '/');

	if (protocol_name != NULL)
		return protocol_name + 1;
	else
		return NULL;
}

GtkTreeModel *
mime_types_model_new (void)
{
	GtkTreeStore *model;
	GList        *type_list;
	GList        *tmp;
	GtkTreeIter   iter;
	GtkTreeIter   child_iter;
	gchar        *mime_type;
	gchar        *path_str;
	const gchar  *description;
	const gchar  *extensions;
	GdkPixbuf    *pixbuf;

	GSList       *url_list;
	GSList       *tmps;
	const gchar  *protocol_name;
	gchar        *protocol_desc;

	gint          i;

	model = gtk_tree_store_new (4, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	tmp = type_list = gnome_vfs_get_registered_mime_types ();

	for (i = 0; categories[i] != NULL; i++) {
		gtk_tree_store_append (model, &iter, NULL);
		gtk_tree_store_set (model, &iter, DESCRIPTION_COLUMN, categories[i], -1);
	}

	for (; tmp != NULL; tmp = tmp->next) {
		mime_type = tmp->data;
		path_str = get_category_path_for_mime_type (mime_type);

		if (path_str != NULL) {
			description = gnome_vfs_mime_get_description (mime_type);
			extensions = gnome_vfs_mime_get_extensions_pretty_string (mime_type);

			if (extensions == NULL || *extensions == '\0')
				continue;

			pixbuf = get_icon_pixbuf (gnome_vfs_mime_get_icon (mime_type));

			get_insertion_point (model, path_str, &iter);

			gtk_tree_store_append (model, &child_iter, &iter);
			gtk_tree_store_set (model, &child_iter,
					    ICON_COLUMN,        pixbuf,
					    DESCRIPTION_COLUMN, description,
					    MIME_TYPE_COLUMN,   mime_type,
					    EXTENSIONS_COLUMN,  extensions,
					    -1);
		}
	}

	g_list_free (type_list);

	tmps = url_list = gconf_client_all_dirs
		(gconf_client_get_default (), "/desktop/gnome/url-handlers", NULL);

	get_insertion_point (model, "Internet Services", &iter);

	for (; tmps != NULL; tmps = tmps->next) {
		protocol_name = get_protocol_name (tmps->data);

		if (protocol_name == NULL)
			continue;

		protocol_desc = get_description_for_protocol (protocol_name);

		gtk_tree_store_append (model, &child_iter, &iter);
		gtk_tree_store_set (model, &child_iter,
				    DESCRIPTION_COLUMN, protocol_desc,
				    MIME_TYPE_COLUMN,   protocol_name,
				    -1);

		if (strcmp (protocol_name, "unknown"))
			gtk_tree_store_set (model, &child_iter, EXTENSIONS_COLUMN,  protocol_name, -1);

		g_free (protocol_desc);
		g_free (tmps->data);
	}

	g_slist_free (url_list);

	return GTK_TREE_MODEL (model);
}

GdkPixbuf *
get_icon_pixbuf (const gchar *short_icon_name) 
{
	gchar *icon_name;
	GdkPixbuf *pixbuf, *pixbuf1;

	static GHashTable *pixbuf_table;

	if (pixbuf_table == NULL)
		pixbuf_table = g_hash_table_new (g_str_hash, g_str_equal);

	if (short_icon_name == NULL)
		short_icon_name = "nautilus/i-regular-24.png";

	icon_name = gnome_program_locate_file
		(gnome_program_get (), GNOME_FILE_DOMAIN_PIXMAP,
		 short_icon_name, TRUE, NULL);

	if (icon_name != NULL) {
		pixbuf1 = g_hash_table_lookup (pixbuf_table, icon_name);

		if (pixbuf1 != NULL) {
			g_object_ref (G_OBJECT (pixbuf1));
		} else {
			pixbuf = gdk_pixbuf_new_from_file (icon_name, NULL);

			if (pixbuf == NULL)
				pixbuf = get_icon_pixbuf (NULL);

			pixbuf1 = gdk_pixbuf_scale_simple (pixbuf, 16, 16, GDK_INTERP_BILINEAR);
			g_object_unref (G_OBJECT (pixbuf));
		}

		g_free (icon_name);
	} else {
		pixbuf1 = get_icon_pixbuf (NULL);
	}

	return pixbuf1;
}

gchar *
get_description_for_protocol (const gchar *protocol_name) 
{
	gchar *description;
	gchar *key;
	int    i;

	key = g_strconcat ("/desktop/gnome/url-handlers/", protocol_name, "/description", NULL);
	description = gconf_client_get_string (gconf_client_get_default (), key, NULL);
	g_free (key);

	if (description != NULL)
		return description;

	for (i = 0; url_descriptions[i][0] != NULL; i++)
		if (!strcmp (url_descriptions[i][0], protocol_name))
			return g_strdup (url_descriptions[i][1]);

	return NULL;
}

gboolean
model_entry_is_protocol (GtkTreeModel *model, GtkTreeIter *iter)
{
	GtkTreeIter parent_iter;
	GtkTreePath *parent_path, *child_path;
	gboolean ret;

	get_insertion_point (GTK_TREE_STORE (model), "Internet Services", &parent_iter);

	parent_path = gtk_tree_model_get_path (model, &parent_iter);
	child_path = gtk_tree_model_get_path (model, iter);
	ret = gtk_tree_path_is_ancestor (parent_path, child_path);
	gtk_tree_path_free (parent_path);
	gtk_tree_path_free (child_path);

	return ret;
}

