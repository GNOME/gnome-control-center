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

#include "service-info.h"
#include "mime-types-model.h"

static gchar *
get_key_name (const ServiceInfo *info, gchar *end) 
{
	return g_strconcat ("/desktop/gnome/url-handlers/", info->protocol, "/", end, NULL);
}

static void
set_string (const ServiceInfo *info, gchar *end, gchar *value, GConfChangeSet *changeset) 
{
	gchar *key;

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

	protocol.g_type = G_TYPE_INVALID;
	gtk_tree_model_get_value (model, iter, MIME_TYPE_COLUMN, &protocol);

	info = g_new0 (ServiceInfo, 1);
	info->model = model;
	info->iter = gtk_tree_iter_copy (iter);
	info->protocol = g_value_dup_string (&protocol);
	info->description = get_string (info, "description", changeset);
	info->run_program = get_bool (info, "type", changeset);
	info->custom_line = get_string (info, "command", changeset);
	info->need_terminal = get_bool (info, "need-terminal", changeset);

	id = get_string (info, "command-id", changeset);
	if (id != NULL)
		info->app = gnome_vfs_mime_application_new_from_id (id);
	g_free (id);

	return info;
}

void
service_info_save (const ServiceInfo *info, GConfChangeSet *changeset)
{
	set_string (info, "description", info->description, changeset);

	if (info->app == NULL) {
		set_string (info, "command", info->custom_line, changeset);
		set_string (info, "command-id", "", changeset);
	} else {
		set_string (info, "command", info->app->command, changeset);
		set_string (info, "command-id", info->app->id, changeset);
	}

	set_bool (info, "type", info->run_program, changeset);
	set_bool (info, "need-terminal", info->need_terminal, changeset);
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
	g_free (info->protocol);
	g_free (info->description);
	gnome_vfs_mime_application_free (info->app);
	g_free (info->custom_line);
}
