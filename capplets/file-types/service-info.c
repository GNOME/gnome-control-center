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

/* This is a hash table of GLists indexed by protocol name; each entry in each
 * list is a GnomeVFSMimeApplication that can handle that protocol */

static GHashTable *service_apps = NULL;

static ModelEntry  *get_services_category_entry (void);

const gchar *url_descriptions[][2] = {
	{ "unknown", N_("Unknown service types") },
	{ "http",    N_("World wide web") },
	{ "ftp",     N_("File transfer protocol") },
	{ "info",    N_("Detailed documentation") },
	{ "man",     N_("Manual pages") },
	{ "mailto",  N_("Electronic mail transmission") },
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
static ModelEntry  *get_services_category_entry (void);
static const gchar *get_protocol_name           (const gchar       *key);



void
load_all_services (void) 
{
	GSList       *url_list;
	GSList       *tmp;
	const gchar  *protocol_name;

	tmp = url_list = gconf_client_all_dirs
		(gconf_client_get_default (), "/desktop/gnome/url-handlers", NULL);

	for (; tmp != NULL; tmp = tmp->next) {
		protocol_name = get_protocol_name (tmp->data);

		if (protocol_name == NULL)
			continue;

		service_info_new (protocol_name, NULL);

		g_free (tmp->data);
	}

	g_slist_free (url_list);
}

ServiceInfo *
service_info_new (const gchar *protocol, GConfChangeSet *changeset)
{
	ServiceInfo *info;

	info = g_new0 (ServiceInfo, 1);
	info->protocol = g_strdup (protocol);
	info->changeset = changeset;

	info->entry.type = MODEL_ENTRY_SERVICE;
	info->entry.parent = MODEL_ENTRY (get_services_category_entry ());
	model_entry_insert_child (get_services_category_entry (), MODEL_ENTRY (info));

	return info;
}

void
service_info_load_all (ServiceInfo *info)
{
	gchar *id;

	service_info_get_description (info);

	info->run_program = get_bool (info, "type");

	if (info->custom_line == NULL)
		info->custom_line = get_string (info, "command");

	info->need_terminal = get_bool (info, "need-terminal");

	if (info->app == NULL) {
		id = get_string (info, "command-id");
		if (id != NULL)
			info->app = gnome_vfs_mime_application_new_from_id (id);
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

void
service_info_set_changeset (ServiceInfo *info, GConfChangeSet *changeset) 
{
	info->changeset = changeset;
}

void
service_info_save (const ServiceInfo *info)
{
	set_string (info, "description", info->description);

	if (info->app == NULL) {
		set_string (info, "command", info->custom_line);
		set_string (info, "command-id", "");
	} else {
		set_string (info, "command", info->app->command);
		set_string (info, "command-id", info->app->id);
	}

	set_bool (info, "type", info->run_program);
	set_bool (info, "need-terminal", info->need_terminal);
}

void
service_info_free (ServiceInfo *info)
{
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
set_string (const ServiceInfo *info, gchar *end, gchar *value) 
{
	gchar *key;

	if (value == NULL)
		return;

	key = get_key_name (info, end);

	if (info->changeset != NULL)
		gconf_change_set_set_string (info->changeset, key, value);
	else
		gconf_client_set_string (gconf_client_get_default (), key, value, NULL);

	g_free (key);
}

static void
set_bool (const ServiceInfo *info, gchar *end, gboolean value) 
{
	gchar *key;

	key = get_key_name (info, end);

	if (info->changeset != NULL)
		gconf_change_set_set_bool (info->changeset, key, value);
	else
		gconf_client_set_bool (gconf_client_get_default (), key, value, NULL);

	g_free (key);
}

static gchar *
get_string (ServiceInfo *info, const gchar *end) 
{
	gchar      *key, *ret;
	GConfValue *value;
	gboolean    found = FALSE;

	key = get_key_name (info, end);

	if (info->changeset != NULL)
		found = gconf_change_set_check_value (info->changeset, key, &value);

	if (!found || info->changeset == NULL) {
		ret = gconf_client_get_string (gconf_client_get_default (), key, NULL);
	} else {
		ret = g_strdup (gconf_value_get_string (value));
		gconf_value_free (value);
	}

	g_free (key);

	return ret;
}

static gboolean
get_bool (const ServiceInfo *info, gchar *end) 
{
	gchar      *key;
	gboolean    ret;
	GConfValue *value;
	gboolean    found = FALSE;

	key = get_key_name (info, end);

	if (info->changeset != NULL)
		found = gconf_change_set_check_value (info->changeset, key, &value);

	if (!found || info->changeset == NULL) {
		ret = gconf_client_get_bool (gconf_client_get_default (), key, NULL);
	} else {
		ret = gconf_value_get_bool (value);
		gconf_value_free (value);
	}

	g_free (key);

	return ret;
}

static ModelEntry *
get_services_category_entry (void) 
{
	static ModelEntry *entry = NULL;

	if (entry == NULL) {
		entry = g_new0 (ModelEntry, 1);
		entry->type = MODEL_ENTRY_SERVICES_CATEGORY;

		model_entry_insert_child (get_model_entries (), entry);
	}

	return entry;
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
