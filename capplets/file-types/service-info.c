/* -*- mode: c; style: linux -*- */

/* service-info.c
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

#include <gconf/gconf-client.h>
#include <libgnomevfs/gnome-vfs-application-registry.h>

#include "service-info.h"
#include "mime-types-model.h"

/* Hash table of service info structures */

static GHashTable *service_info_table = NULL;

/* This is a hash table of GLists indexed by protocol name; each entry in each
 * list is a GnomeVFSMimeApplication that can handle that protocol */

static GHashTable *service_apps = NULL;

static gchar *
get_key_name (const ServiceInfo *info, gchar *end) 
{
	return g_strconcat ("/desktop/gnome/url-handlers/", info->protocol, "/", end, NULL);
}

static void
fill_service_apps (void) 
{
	GList *apps, *tmp, *tmp1;
	const gchar *uri_schemes_str;
	gchar **uri_schemes;
	int i;

	if (service_apps == NULL)
		service_apps = g_hash_table_new (g_str_hash, g_str_equal);

	/* FIXME: This currently returns NULL. We need a way to retrieve all
	   apps in the registry */
	apps = gnome_vfs_application_registry_get_applications ("*/*");

	for (tmp = apps; tmp != NULL; tmp = tmp->next) {
		uri_schemes_str = gnome_vfs_application_registry_peek_value (tmp->data, "supported_uri_schemes");
		uri_schemes = g_strsplit (uri_schemes_str, ",", -1);

		for (i = 0; uri_schemes[i] != NULL; i++) {
			tmp1 = g_hash_table_lookup (service_apps, uri_schemes[i]);
			tmp1 = g_list_prepend (tmp1, gnome_vfs_application_registry_get_mime_application (tmp->data));
			g_hash_table_remove (service_apps, uri_schemes[i]);
			g_hash_table_insert (service_apps, uri_schemes[i], tmp1);
		}

		g_strfreev (uri_schemes);
	}

	g_list_foreach (apps, (GFunc) g_free, NULL);
	g_list_free (apps);
}

static void
set_string (const ServiceInfo *info, gchar *end, gchar *value, GConfChangeSet *changeset) 
{
	gchar *key;

	if (value == NULL)
		return;

	key = get_key_name (info, end);

	if (changeset != NULL)
		gconf_change_set_set_string (changeset, key, value);
	else
		gconf_client_set_string (gconf_client_get_default (), key, value, NULL);

	g_free (key);
}

static void
set_bool (const ServiceInfo *info, gchar *end, gboolean value, GConfChangeSet *changeset) 
{
	gchar *key;

	key = get_key_name (info, end);

	if (changeset != NULL)
		gconf_change_set_set_bool (changeset, key, value);
	else
		gconf_client_set_bool (gconf_client_get_default (), key, value, NULL);

	g_free (key);
}

static gchar *
get_string (const ServiceInfo *info, gchar *end, GConfChangeSet *changeset) 
{
	gchar      *key, *ret;
	GConfValue *value;
	gboolean    found;

	key = get_key_name (info, end);

	if (changeset != NULL)
		found = gconf_change_set_check_value (changeset, key, &value);

	if (!found || changeset == NULL) {
		if (!strcmp (end, "description"))
			ret = get_description_for_protocol (info->protocol);
		else
			ret = gconf_client_get_string (gconf_client_get_default (), key, NULL);
	} else {
		ret = g_strdup (gconf_value_get_string (value));
		gconf_value_free (value);
	}

	g_free (key);

	return ret;
}

static gboolean
get_bool (const ServiceInfo *info, gchar *end, GConfChangeSet *changeset) 
{
	gchar      *key;
	gboolean    ret;
	GConfValue *value;
	gboolean    found;

	key = get_key_name (info, end);

	if (changeset != NULL)
		found = gconf_change_set_check_value (changeset, key, &value);

	if (!found || changeset == NULL) {
		ret = gconf_client_get_bool (gconf_client_get_default (), key, NULL);
	} else {
		ret = gconf_value_get_bool (value);
		gconf_value_free (value);
	}

	g_free (key);

	return ret;
}

ServiceInfo *
service_info_load (GtkTreeModel *model, GtkTreeIter *iter, GConfChangeSet *changeset)
{
	ServiceInfo *info;
	gchar       *id;
	GValue       protocol;

	if (service_info_table == NULL)
		service_info_table = g_hash_table_new (g_str_hash, g_str_equal);

	protocol.g_type = G_TYPE_INVALID;
	gtk_tree_model_get_value (model, iter, MIME_TYPE_COLUMN, &protocol);

	info = g_hash_table_lookup (service_info_table, g_value_get_string (&protocol));

	if (info != NULL) {
		g_value_unset (&protocol);
		return info;
	}

	info = g_new0 (ServiceInfo, 1);
	info->model = model;
	info->iter = gtk_tree_iter_copy (iter);
	info->changeset = changeset;
	info->protocol = g_value_dup_string (&protocol);
	info->description = get_string (info, "description", changeset);
	info->run_program = get_bool (info, "type", changeset);
	info->custom_line = get_string (info, "command", changeset);
	info->need_terminal = get_bool (info, "need-terminal", changeset);

	id = get_string (info, "command-id", changeset);
	if (id != NULL)
		info->app = gnome_vfs_mime_application_new_from_id (id);
	g_free (id);

	g_hash_table_insert (service_info_table, info->protocol, info);
	g_value_unset (&protocol);

	return info;
}

void
service_info_save (const ServiceInfo *info)
{
	set_string (info, "description", info->description, info->changeset);

	if (info->app == NULL) {
		set_string (info, "command", info->custom_line, info->changeset);
		set_string (info, "command-id", "", info->changeset);
	} else {
		set_string (info, "command", info->app->command, info->changeset);
		set_string (info, "command-id", info->app->id, info->changeset);
	}

	set_bool (info, "type", info->run_program, info->changeset);
	set_bool (info, "need-terminal", info->need_terminal, info->changeset);
}

void
service_info_update (ServiceInfo *info) 
{
	gtk_tree_store_set (GTK_TREE_STORE (info->model), info->iter,
			    DESCRIPTION_COLUMN, info->description,
			    -1);
}

void
service_info_free (ServiceInfo *info)
{
	g_hash_table_remove (service_info_table, info->protocol);

	g_free (info->protocol);
	g_free (info->description);
	gnome_vfs_mime_application_free (info->app);
	g_free (info->custom_line);
	g_free (info);
}

const GList *
get_apps_for_service_type (gchar *protocol) 
{
	if (service_apps == NULL)
		fill_service_apps ();

	return g_hash_table_lookup (service_apps, protocol);
}
