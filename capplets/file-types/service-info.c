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

#include <string.h>
#include <gconf/gconf-client.h>
#include <libgnomevfs/gnome-vfs-application-registry.h>

#include "libuuid/uuid.h"

#include "service-info.h"
#include "mime-types-model.h"

/* This is a hash table of GLists indexed by protocol name; each entry in each
 * list is a GnomeVFSMimeApplication that can handle that protocol */

static GHashTable *service_apps = NULL;

const gchar *url_descriptions[][2] = {
	{ "unknown", N_("Unknown service types") },
	{ "http",    N_("World wide web") },
	{ "ftp",     N_("File transfer protocol") },
	{ "info",    N_("Detailed documentation") },
	{ "man",     N_("Manual pages") },
	{ "mailto",  N_("Electronic mail transmission") },
	{ "ghelp",   N_("Gnome documentation") },
	{ NULL,      NULL }
};

static gchar       *get_key_name                (const ServiceInfo *info,
						 const gchar       *end);
static void         fill_service_apps           (void);
static void         set_string                  (const ServiceInfo *info,
						 gchar             *end,
						 gchar             *value);
static void         set_bool                    (const ServiceInfo *info,
						 gchar             *end,
						 gboolean           value);
static gchar       *get_string                  (ServiceInfo       *info,
						 const gchar       *end);
static gboolean     get_bool                    (const ServiceInfo *info,
						 gchar             *end);
static const gchar *get_protocol_name           (const gchar       *key);

void
load_all_services (GtkTreeModel *model) 
{
	GSList       *urls;
	const gchar  *protocol_name;
	ServiceInfo  *info;

	urls = gconf_client_all_dirs (gconf_client_get_default (), "/desktop/gnome/url-handlers", NULL);

	while (urls) {
		protocol_name = get_protocol_name (urls->data);

		if (protocol_name == NULL)
			continue;

		info = service_info_new (protocol_name, model);
		model_entry_insert_child (get_services_category_entry (model), MODEL_ENTRY (info), model);

		g_free (urls->data);
		urls = g_slist_remove (urls, urls->data);
	}
}

ServiceInfo *
service_info_new (const gchar *protocol, GtkTreeModel *model)
{
	ServiceInfo *info;

	info = g_new0 (ServiceInfo, 1);

	if (protocol != NULL)
		info->protocol = g_strdup (protocol);

	info->entry.type = MODEL_ENTRY_SERVICE;
	info->entry.parent = MODEL_ENTRY (get_services_category_entry (model));

	return info;
}

void
service_info_load_all (ServiceInfo *info)
{
	gchar *id;

	service_info_get_description (info);

#if 0
	info->run_program = get_bool (info, "type");
#else
	info->run_program = TRUE;
#endif

	if (info->app == NULL) {
		id = get_string (info, "command-id");
		if (id != NULL)
			info->app = gnome_vfs_mime_application_new_from_id (id);
		else {
			info->app = g_new0 (GnomeVFSMimeApplication, 1);
			info->app->command = get_string (info, "command");
			info->app->requires_terminal = get_bool (info, "needs-terminal");
		}
		g_free (id);
	}
}

const gchar *
service_info_get_description (ServiceInfo *info) 
{
	int i;

	if (info->description == NULL) {
		info->description = get_string (info, "description");

		if (info->description != NULL)
			return info->description;

		for (i = 0; url_descriptions[i][0] != NULL; i++)
			if (!strcmp (url_descriptions[i][0], info->protocol))
				return g_strdup (url_descriptions[i][1]);
	}

	return info->description;
}

gboolean
service_info_using_custom_app (const ServiceInfo *info)
{
	gchar *tmp;
	gboolean ret;

	if (!info->app) return FALSE;

	if (info->app->name == NULL)
		return TRUE;

	tmp = g_strdup_printf ("Custom %s", info->protocol);
	ret = !strcmp (tmp, info->app->name);
	g_free (tmp);

	return ret;
}

void
service_info_save (const ServiceInfo *info)
{
	gchar  *tmp;
	uuid_t  app_uuid;
	gchar   app_uuid_str[100];

	set_string (info, "description", info->description);

	if (info->app != NULL && info->app->command != NULL && *info->app->command != '\0') {
		tmp = g_strdup_printf ("Custom %s", info->protocol);

		if (info->app->name == NULL)
			info->app->name = tmp;

		if (info->app->id == NULL) {
			uuid_generate (app_uuid);
			uuid_unparse (app_uuid, app_uuid_str);

			info->app->id = g_strdup (app_uuid_str);

			gnome_vfs_application_registry_save_mime_application (info->app);
			gnome_vfs_application_registry_sync ();
		}
		else if (!strcmp (tmp, info->app->name)) {
			gnome_vfs_application_registry_set_value (info->app->id, "command",
								  info->app->command);
			gnome_vfs_application_registry_set_bool_value (info->app->id, "requires_terminal",
								       info->app->requires_terminal);
		}

		set_string (info, "command", info->app->command);
		set_string (info, "command-id", info->app->id);
	} else {
		set_string (info, "command", NULL);
		set_string (info, "command-id", NULL);
	}

	set_bool (info, "type", info->run_program);
}

void
service_info_delete (const ServiceInfo *info)
{
	gchar *tmp;

	tmp = get_key_name (info, "type");
	gconf_client_unset (gconf_client_get_default (), tmp, NULL);
	g_free (tmp);

	tmp = get_key_name (info, "description");
	gconf_client_unset (gconf_client_get_default (), tmp, NULL);
	g_free (tmp);

	tmp = get_key_name (info, "command");
	gconf_client_unset (gconf_client_get_default (), tmp, NULL);
	g_free (tmp);

	tmp = get_key_name (info, "command-id");
	gconf_client_unset (gconf_client_get_default (), tmp, NULL);
	g_free (tmp);

	tmp = get_key_name (info, "need-terminal");
	gconf_client_unset (gconf_client_get_default (), tmp, NULL);
	g_free (tmp);
}

void
service_info_free (ServiceInfo *info)
{
	g_free (info->protocol);
	g_free (info->description);
	gnome_vfs_mime_application_free (info->app);
	g_free (info);
}

const GList *
get_apps_for_service_type (gchar *protocol) 
{
	if (service_apps == NULL)
		fill_service_apps ();

	return g_hash_table_lookup (service_apps, protocol);
}

ModelEntry *
get_services_category_entry (GtkTreeModel *model) 
{
	static ModelEntry *entry = NULL;

	if (entry == NULL) {
		entry = g_new0 (ModelEntry, 1);
		entry->type = MODEL_ENTRY_SERVICES_CATEGORY;
		entry->parent = get_model_entries (model);

		model_entry_insert_child (get_model_entries (model), entry, model);
	}

	return entry;
}

ServiceInfo *
get_service_info (const gchar *protocol)
{
	ModelEntry *tmp;

	for (tmp = get_services_category_entry (NULL)->first_child; tmp != NULL; tmp = tmp->next)
		if (tmp->type == MODEL_ENTRY_SERVICE && !strcmp (SERVICE_INFO (tmp)->protocol, protocol))
			break;

	return SERVICE_INFO (tmp);
}



static gchar *
get_key_name (const ServiceInfo *info, const gchar *end) 
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

	apps = gnome_vfs_application_registry_get_applications (NULL);
	for (tmp = apps; tmp != NULL; tmp = tmp->next) {
		uri_schemes_str = gnome_vfs_application_registry_peek_value (tmp->data, "supported_uri_schemes");
		if (uri_schemes_str == NULL)
			continue;

		uri_schemes = g_strsplit (uri_schemes_str, ",", -1);
		if (uri_schemes == NULL)
			continue;

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
set_string (const ServiceInfo *info, gchar *end, gchar *value) 
{
	gchar *key;

	if (value == NULL)
		return;

	key = get_key_name (info, end);
	gconf_client_set_string (gconf_client_get_default (), key, value, NULL);
	g_free (key);
}

static void
set_bool (const ServiceInfo *info, gchar *end, gboolean value) 
{
	gchar *key;

	key = get_key_name (info, end);
	gconf_client_set_bool (gconf_client_get_default (), key, value, NULL);
	g_free (key);
}

static gchar *
get_string (ServiceInfo *info, const gchar *end) 
{
	gchar *key, *ret;

	key = get_key_name (info, end);
	ret = gconf_client_get_string (gconf_client_get_default (), key, NULL);
	g_free (key);

	return ret;
}

static gboolean
get_bool (const ServiceInfo *info, gchar *end) 
{
	gchar      *key;
	gboolean    ret;

	key = get_key_name (info, end);
	ret = gconf_client_get_bool (gconf_client_get_default (), key, NULL);
	g_free (key);

	return ret;
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
