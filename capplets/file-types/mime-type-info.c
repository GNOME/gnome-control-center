/* -*- mode: c; style: linux -*- */

/* mime-type-info.c
 *
 * Copyright (C) 2000 Eazel, Inc.
 * Copyright (C) 2002 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>,
 *            Jonathan Blandford <jrb@redhat.com>,
 *            Gene Z. Ragan <gzr@eazel.com>
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

#include <bonobo.h>
#include <libgnomevfs/gnome-vfs-application-registry.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <gconf/gconf-client.h>
#include <ctype.h>

#include "libuuid/uuid.h"

#include "mime-type-info.h"
#include "mime-types-model.h"

static const gchar *get_category_name (const gchar        *mime_type);
static GSList *get_lang_list          (void);
static gchar  *form_extensions_string (const MimeTypeInfo *info,
				       gchar              *sep,
				       gchar              *prepend);
static void    get_icon_pixbuf        (MimeTypeInfo       *info,
				       const gchar        *icon_path,
				       gboolean            want_large);

static MimeCategoryInfo *get_category (const gchar        *category_name,
				       GtkTreeModel       *model);



void
load_all_mime_types (GtkTreeModel *model) 
{
	GList *list, *tmp;

	list = gnome_vfs_get_registered_mime_types ();

	for (tmp = list; tmp != NULL; tmp = tmp->next)
		mime_type_info_new (tmp->data, model);

	g_list_free (list);
}

MimeTypeInfo *
mime_type_info_new (const gchar *mime_type, GtkTreeModel *model)
{
	MimeTypeInfo      *info;

	info = g_new0 (MimeTypeInfo, 1);
	MODEL_ENTRY (info)->type = MODEL_ENTRY_MIME_TYPE;

	if (mime_type != NULL) {
		info->mime_type = g_strdup (mime_type);

		mime_type_info_set_category_name (info, get_category_name (mime_type), model);
	} else {
		info->entry.parent = get_model_entries (model);
	}

	return info;
}

/* Fill in the remaining fields in a MimeTypeInfo structure; suitable for
 * subsequent use in an edit dialog */

void
mime_type_info_load_all (MimeTypeInfo *info)
{
	gchar *tmp;
	const gchar *tmp1;

	mime_type_info_get_description (info);
	mime_type_info_get_file_extensions (info);

	if (info->default_action == NULL)
		info->default_action = gnome_vfs_mime_get_default_application (info->mime_type);

	if (info->icon_name == NULL)
		info->icon_name = g_strdup (gnome_vfs_mime_get_icon (info->mime_type));

	if (info->icon_pixbuf == NULL)
		get_icon_pixbuf (info, info->icon_name, TRUE);

	if (info->custom_line == NULL) {
		tmp = g_strdup_printf ("Custom %s", info->mime_type);

		if (info->default_action != NULL && !strcmp (info->default_action->name, tmp)) {
			info->custom_line = g_strdup (info->default_action->command);
			info->needs_terminal = gnome_vfs_application_registry_get_bool_value
				(info->default_action->id, "requires_terminal", NULL);
			gnome_vfs_mime_application_free (info->default_action);
			info->default_action = NULL;
		}

		g_free (tmp);
	}

	if (info->default_component == NULL)
		info->default_component = gnome_vfs_mime_get_default_component (info->mime_type);

	if (!info->use_cat_loaded) {
		tmp1 = gnome_vfs_mime_get_value (info->mime_type, "use-category");

		if (tmp1 != NULL && !strcmp (tmp1, "yes"))
			info->use_category = TRUE;
		else
			info->use_category = FALSE;

		info->use_cat_loaded = TRUE;
	}
}

const gchar *
mime_type_info_get_description (MimeTypeInfo *info)
{
	if (info->description == NULL)
		info->description = g_strdup (gnome_vfs_mime_get_description (info->mime_type));

	return info->description;
}

GdkPixbuf *
mime_type_info_get_icon (MimeTypeInfo *info)
{
	if (info->small_icon_pixbuf == NULL)
		get_icon_pixbuf (info, mime_type_info_get_icon_path (info), FALSE);

	g_object_ref (G_OBJECT (info->small_icon_pixbuf));

	return info->small_icon_pixbuf;
}

const gchar *
mime_type_info_get_icon_path (MimeTypeInfo *info) 
{
	gchar *tmp;

	if (info->icon_name == NULL)
		info->icon_name = g_strdup (gnome_vfs_mime_get_icon (info->mime_type));

	if (g_file_exists (info->icon_name)) {
		info->icon_path = g_strdup (info->icon_name);
		return info->icon_path;
	}

	info->icon_path = gnome_vfs_icon_path_from_filename (info->icon_name);

	if (info->icon_path == NULL) {
		tmp = g_strconcat (info->icon_name, ".png", NULL);
		info->icon_path = gnome_vfs_icon_path_from_filename (tmp);
		g_free (tmp);
	}

	if (info->icon_path == NULL) {
		tmp = g_strconcat ("nautilus/", info->icon_name, NULL);
		info->icon_path = gnome_vfs_icon_path_from_filename (tmp);
		g_free (tmp);
	}

	if (info->icon_path == NULL) {
		tmp = g_strconcat ("nautilus/", info->icon_name, ".png", NULL);
		info->icon_path = gnome_vfs_icon_path_from_filename (tmp);
		g_free (tmp);
	}

	if (info->icon_path == NULL)
		info->icon_path = gnome_vfs_icon_path_from_filename ("nautilus/i-regular-24.png");

	return info->icon_path;
}

const GList *
mime_type_info_get_file_extensions (MimeTypeInfo *info)
{
	if (info->file_extensions == NULL)
		info->file_extensions = gnome_vfs_mime_get_extensions_list (info->mime_type);

	return info->file_extensions;
}

gchar *
mime_type_info_get_file_extensions_pretty_string (MimeTypeInfo *info)
{
	mime_type_info_get_file_extensions (info);

	return form_extensions_string (info, ", ", ".");
}

gchar *
mime_type_info_get_category_name (const MimeTypeInfo *info)
{
	return mime_category_info_get_full_name (MIME_CATEGORY_INFO (info->entry.parent));
}

void
mime_type_info_set_category_name (const MimeTypeInfo *info, const gchar *category_name, GtkTreeModel *model) 
{
	if (MODEL_ENTRY (info)->parent != NULL)
		model_entry_remove_child (MODEL_ENTRY (info)->parent, MODEL_ENTRY (info), model);

	if (category_name != NULL) {
		MODEL_ENTRY (info)->parent = MODEL_ENTRY (get_category (category_name, model));

		if (MODEL_ENTRY (info)->parent != NULL)
			model_entry_insert_child (MODEL_ENTRY (info)->parent, MODEL_ENTRY (info), model);
	} else {
		MODEL_ENTRY (info)->parent = NULL;
	}
}

void
mime_type_info_set_file_extensions (MimeTypeInfo *info, GList *list)
{
	/* FIXME: Free the old list */
	info->file_extensions = list;
}

void
mime_type_info_save (const MimeTypeInfo *info)
{
	gchar                   *tmp;
	uuid_t                   app_uuid;
	gchar                    app_uuid_str[100];
	GnomeVFSMimeApplication  app;

	gnome_vfs_mime_set_description (info->mime_type, info->description);
	gnome_vfs_mime_set_icon (info->mime_type, info->icon_name);

	if (info->default_action != NULL) {
		gnome_vfs_mime_set_default_application (info->mime_type, info->default_action->id);
	}
	else if (info->custom_line != NULL && *info->custom_line != '\0') {
		uuid_generate (app_uuid);
		uuid_unparse (app_uuid, app_uuid_str);

		app.id = app_uuid_str;
		app.name = g_strdup_printf ("Custom %s", info->mime_type);
		app.command = info->custom_line;
		app.can_open_multiple_files = FALSE;
		app.expects_uris = FALSE;
		app.supported_uri_schemes = NULL;
		app.requires_terminal = info->needs_terminal;

		gnome_vfs_application_registry_save_mime_application (&app);
		gnome_vfs_application_registry_sync ();

		gnome_vfs_mime_set_default_application (info->mime_type, app.id);
		g_free (app.name);
	}

	tmp = form_extensions_string (info, " ", NULL);
	gnome_vfs_mime_set_extensions_list (info->mime_type, tmp);
	g_free (tmp);

	if (info->default_component != NULL)
		gnome_vfs_mime_set_default_component (info->mime_type, info->default_component->iid);
	else
		gnome_vfs_mime_set_default_component (info->mime_type, NULL);

	tmp = mime_type_info_get_category_name (info);
	gnome_vfs_mime_set_value (info->mime_type, "category", tmp);
	g_free (tmp);

	gnome_vfs_mime_set_value (info->mime_type, "use-category", info->use_category ? "yes" : "no");
}

void
mime_type_info_free (MimeTypeInfo *info)
{
	g_free (info->mime_type);
	g_free (info->description);
	g_free (info->icon_name);
	g_free (info->icon_path);
	gnome_vfs_mime_extensions_list_free (info->file_extensions);
	CORBA_free (info->default_component);
	gnome_vfs_mime_application_free (info->default_action);
	g_free (info->custom_line);

	if (info->icon_pixbuf != NULL)
		g_object_unref (G_OBJECT (info->icon_pixbuf));
	if (info->small_icon_pixbuf != NULL)
		g_object_unref (G_OBJECT (info->small_icon_pixbuf));

	g_free (info);
}

MimeCategoryInfo *
mime_category_info_new (MimeCategoryInfo *parent, const gchar *name, GtkTreeModel *model) 
{
	MimeCategoryInfo      *info;

	info = g_new0 (MimeCategoryInfo, 1);
	MODEL_ENTRY (info)->type = MODEL_ENTRY_CATEGORY;

	info->name = g_strdup (name);

	if (parent != NULL)
		MODEL_ENTRY (info)->parent = MODEL_ENTRY (parent);
	else
		MODEL_ENTRY (info)->parent = get_model_entries (model);

	return info;
}

static gchar *
get_gconf_base_name (MimeCategoryInfo *category) 
{
	gchar *tmp, *tmp1;

	tmp1 = mime_category_info_get_full_name (category);

	for (tmp = tmp1; *tmp != '\0'; tmp++)
		if (isspace (*tmp)) *tmp = '-';

	tmp = g_strconcat ("/desktop/gnome/file-types-categories/",
			   tmp1, "/default-action-id", NULL);

	g_free (tmp1);
	return tmp;
}

void
mime_category_info_load_all (MimeCategoryInfo *category)
{
	gchar *tmp, *tmp1;
	gchar *appid;
	GnomeVFSMimeApplication *app;

	tmp1 = get_gconf_base_name (category);
	tmp = g_strconcat (tmp1, "/default-action-id", NULL);
	appid = gconf_client_get_string (gconf_client_get_default (), tmp, NULL);
	g_free (tmp);

	if (appid != NULL && *appid != '\0') {
		tmp = g_strdup_printf ("Custom %s", category->name);
		app = gnome_vfs_application_registry_get_mime_application (appid);
		if (!strcmp (app->name, tmp)) {
			category->default_action = NULL;
			category->custom_line = app->command;
			category->needs_terminal = app->requires_terminal;
			gnome_vfs_mime_application_free (app);
		} else {
			category->default_action = app;
			category->custom_line = NULL;
		}
	} else {
		category->default_action = NULL;
		category->custom_line = NULL;
	}

	if (!category->use_parent_cat_loaded) {
		if (category->entry.parent->type == MODEL_ENTRY_CATEGORY) {
			tmp = g_strconcat (tmp1, "/use-parent-category", NULL);
			category->use_parent_category = gconf_client_get_bool (gconf_client_get_default (), tmp, NULL);
			g_free (tmp);
		} else {
			category->use_parent_category = FALSE;
		}

		category->use_parent_cat_loaded = TRUE;
	}

	g_free (tmp1);
}

static void
set_subcategory_ids (ModelEntry *entry, MimeCategoryInfo *category, gchar *app_id) 
{
	ModelEntry *tmp;

	switch (entry->type) {
	case MODEL_ENTRY_MIME_TYPE:
		if (MIME_TYPE_INFO (entry)->use_category) {
			gnome_vfs_mime_set_default_application (MIME_TYPE_INFO (entry)->mime_type, app_id);
			gnome_vfs_mime_application_free (MIME_TYPE_INFO (entry)->default_action);

			if (category->default_action == NULL)
				MIME_TYPE_INFO (entry)->default_action = NULL;
			else
				MIME_TYPE_INFO (entry)->default_action = gnome_vfs_mime_application_copy (category->default_action);

			g_free (MIME_TYPE_INFO (entry)->custom_line);

			if (app_id == NULL)
				MIME_TYPE_INFO (entry)->custom_line = NULL;
			else
				MIME_TYPE_INFO (entry)->custom_line = g_strdup (category->custom_line);

			MIME_TYPE_INFO (entry)->needs_terminal = category->needs_terminal;
		}

		break;

	case MODEL_ENTRY_CATEGORY:
		if (entry != MODEL_ENTRY (category) && MIME_CATEGORY_INFO (entry)->use_parent_category)
			for (tmp = entry->first_child; tmp != NULL; tmp = tmp->next)
				set_subcategory_ids (tmp, category, app_id);
		break;

	default:
		break;
	}
}

void
mime_category_info_save (MimeCategoryInfo *category) 
{
	gchar                   *tmp, *tmp1;
	gchar                   *app_id;
	uuid_t                   app_uuid;
	gchar                    app_uuid_str[100];
	GnomeVFSMimeApplication  app;

	tmp1 = get_gconf_base_name (category);
	tmp = g_strconcat (tmp1, "/default-action-id", NULL);

	if (category->default_action != NULL) {
		gconf_client_set_string (gconf_client_get_default (),
					 tmp, category->default_action->id, NULL);
		app_id = category->default_action->id;
	}
	else if (category->custom_line != NULL && *category->custom_line != '\0') {
		uuid_generate (app_uuid);
		uuid_unparse (app_uuid, app_uuid_str);

		app.id = app_uuid_str;
		app.name = g_strdup_printf ("Custom %s", category->name);
		app.command = category->custom_line;
		app.can_open_multiple_files = FALSE;
		app.expects_uris = FALSE;
		app.supported_uri_schemes = NULL;
		app.requires_terminal = category->needs_terminal;

		gnome_vfs_application_registry_save_mime_application (&app);
		gnome_vfs_application_registry_sync ();

		gconf_client_set_string (gconf_client_get_default (),
					 tmp, app.id, NULL);
		g_free (app.name);
		app_id = app_uuid_str;
	} else {
		app_id = NULL;
	}

	g_free (tmp);

	tmp1 = mime_category_info_get_full_name (category);
	tmp = g_strconcat (tmp1, "/use-parent-category", NULL);
	gconf_client_set_bool (gconf_client_get_default (), tmp, category->use_parent_category, NULL);
	g_free (tmp1);

	if (app_id != NULL)
		set_subcategory_ids (MODEL_ENTRY (category), category, app_id);
}

static GList *
find_possible_supported_apps (ModelEntry *entry, gboolean top) 
{
	GList      *ret;
	ModelEntry *tmp;

	if (entry == NULL) return NULL;

	switch (entry->type) {
	case MODEL_ENTRY_CATEGORY:
		if (!top && !MIME_CATEGORY_INFO (entry)->use_parent_category)
			return NULL;

		for (tmp = entry->first_child; tmp != NULL; tmp = tmp->next) {
			ret = find_possible_supported_apps (tmp, FALSE);

			if (ret != NULL)
				return ret;
		}

		return NULL;

	case MODEL_ENTRY_MIME_TYPE:
		if (MIME_TYPE_INFO (entry)->use_category)
			return gnome_vfs_application_registry_get_applications (MIME_TYPE_INFO (entry)->mime_type);
		else
			return NULL;

	default:
		return NULL;
	}
}

static GList *
intersect_lists (GList *list, GList *list1) 
{
	GList *tmp, *tmp1, *tmpnext;

	tmp = list;

	while (tmp != NULL) {
		tmpnext = tmp->next;

		for (tmp1 = list1; tmp1 != NULL; tmp1 = tmp1->next)
			if (!strcmp (tmp->data, tmp1->data))
				break;

		if (tmp1 == NULL)
			list = g_list_remove_link (list, tmp);

		tmp = tmpnext;
	}

	return list;
}

static GList *
reduce_supported_app_list (ModelEntry *entry, GList *list, gboolean top) 
{
	GList      *type_list;
	ModelEntry *tmp;

	switch (entry->type) {
	case MODEL_ENTRY_CATEGORY:
		if (!top && !MIME_CATEGORY_INFO (entry)->use_parent_category)
			break;

		for (tmp = entry->first_child; tmp != NULL; tmp = tmp->next)
			list = reduce_supported_app_list (tmp, list, FALSE);
		break;

	case MODEL_ENTRY_MIME_TYPE:
		if (MIME_TYPE_INFO (entry)->use_category) {
			type_list = gnome_vfs_application_registry_get_applications (MIME_TYPE_INFO (entry)->mime_type);
			list = intersect_lists (list, type_list);
			g_list_free (type_list);
		}

		break;

	default:
		break;
	}

	return list;
}

GList *
mime_category_info_find_apps (MimeCategoryInfo *info)
{
	GList *ret;

	ret = find_possible_supported_apps (MODEL_ENTRY (info), TRUE);
	return reduce_supported_app_list (MODEL_ENTRY (info), ret, TRUE);
}

gchar *
mime_category_info_get_full_name (MimeCategoryInfo *info) 
{
	GString *string;
	ModelEntry *tmp;
	gchar *ret;

	string = g_string_new ("");

	for (tmp = MODEL_ENTRY (info); tmp != NULL && tmp->type != MODEL_ENTRY_NONE; tmp = tmp->parent) {
		g_string_prepend (string, MIME_CATEGORY_INFO (tmp)->name);
		g_string_prepend (string, "/");
	}

	ret = g_strdup ((*string->str == '\0') ? string->str : string->str + 1);
	g_string_free (string, TRUE);
	return ret;
}

char *
mime_type_get_pretty_name_for_server (Bonobo_ServerInfo *server)
{
        const char *view_as_name;       
	char       *display_name;
        GSList     *langs;

        display_name = NULL;

        langs = get_lang_list ();
        view_as_name = bonobo_server_info_prop_lookup (server, "nautilus:view_as_name", langs);
		
        if (view_as_name == NULL)
                view_as_name = bonobo_server_info_prop_lookup (server, "name", langs);

        if (view_as_name == NULL)
                view_as_name = server->iid;
       
	g_slist_foreach (langs, (GFunc) g_free, NULL);
        g_slist_free (langs);

	/* if the name is an OAFIID, clean it up for display */
	if (!strncmp (view_as_name, "OAFIID:", strlen ("OAFIID:"))) {
		char *display_name, *colon_ptr;

		display_name = g_strdup (view_as_name + strlen ("OAFIID:"));
		colon_ptr = strchr (display_name, ':');
		if (colon_ptr)
			*colon_ptr = '\0';

		return display_name;					
	}
			
        return g_strdup_printf ("View as %s", view_as_name);
}

static MimeTypeInfo *
get_mime_type_info_int (ModelEntry *entry, const gchar *mime_type) 
{
	ModelEntry *tmp;
	MimeTypeInfo *ret;

	switch (entry->type) {
	case MODEL_ENTRY_MIME_TYPE:
		if (!strcmp (MIME_TYPE_INFO (entry)->mime_type, mime_type))
			return MIME_TYPE_INFO (entry);

		return NULL;

	case MODEL_ENTRY_CATEGORY:
	case MODEL_ENTRY_NONE:
		for (tmp = entry->first_child; tmp != NULL; tmp = tmp->next)
			if ((ret = get_mime_type_info_int (tmp, mime_type)) != NULL)
				return ret;

		return NULL;

	default:
		return NULL;
	}
}

MimeTypeInfo *
get_mime_type_info (const gchar *mime_type)
{
	return get_mime_type_info_int (get_model_entries (NULL), mime_type);
}



static const gchar *
get_category_name (const gchar *mime_type) 
{
	const gchar *path;

	path = gnome_vfs_mime_get_value (mime_type, "category");

	if (path != NULL)
		return g_strdup (path);
	else if (!strncmp (mime_type, "image/", strlen ("image/")))
		return "Images";
	else if (!strncmp (mime_type, "video/", strlen ("video/")))
		return "Video";
	else if (!strncmp (mime_type, "audio/", strlen ("audio/")))
		return "Audio";
	else
		return NULL;
}

static GSList *
get_lang_list (void)
{
        GSList *retval;
        const char *lang;
        char *equal_char;

        retval = NULL;

        lang = g_getenv ("LANGUAGE");

        if (lang == NULL)
                lang = g_getenv ("LANG");


        if (lang != NULL) {
                equal_char = strchr (lang, '=');
                if (equal_char != NULL)
                        lang = equal_char + 1;

                retval = g_slist_prepend (retval, g_strdup (lang));
        }
        
        return retval;
}

static gchar *
form_extensions_string (const MimeTypeInfo *info, gchar *sep, gchar *prepend) 
{
	gchar *tmp;
	gchar **array;
	GList *l;
	gint i = 0;

	if (prepend == NULL)
		prepend = "";

	array = g_new0 (gchar *, g_list_length (info->file_extensions) + 1);
	for (l = info->file_extensions; l != NULL; l = l->next)
		array[i++] = g_strconcat (prepend, l->data, NULL);
	tmp = g_strjoinv (sep, array);
	g_strfreev (array);

	return tmp;
}

/* Loads a pixbuf for the icon, falling back on the default icon if
 * necessary
 */

void
get_icon_pixbuf (MimeTypeInfo *info, const gchar *icon_path, gboolean want_large) 
{
	static GHashTable *icon_table = NULL;

	if (icon_path == NULL)
		icon_path = gnome_vfs_icon_path_from_filename ("nautilus/i-regular-24.png");

	if ((want_large && info->icon_pixbuf != NULL) || info->small_icon_pixbuf != NULL)
		return;

	if (icon_table == NULL)
		icon_table = g_hash_table_new (g_str_hash, g_str_equal);

	if (!want_large)
		info->small_icon_pixbuf = g_hash_table_lookup (icon_table, icon_path);

	if (info->small_icon_pixbuf != NULL) {
		g_object_ref (G_OBJECT (info->small_icon_pixbuf));
	} else {
		info->icon_pixbuf = gdk_pixbuf_new_from_file (icon_path, NULL);

		if (info->icon_pixbuf == NULL) {
			get_icon_pixbuf (info, NULL, want_large);
		}
		else if (!want_large) {
			info->small_icon_pixbuf =
				gdk_pixbuf_scale_simple (info->icon_pixbuf, 16, 16, GDK_INTERP_HYPER);

			g_hash_table_insert (icon_table, g_strdup (icon_path), info->small_icon_pixbuf);
		}
	}
}

static MimeCategoryInfo *
get_category (const gchar *category_name, GtkTreeModel *model)
{
	ModelEntry *current, *child;
	gchar **categories;
	int i;

	if (category_name == NULL)
		return NULL;

	categories = g_strsplit (category_name, "/", -1);

	current = get_model_entries (model);

	for (i = 0; categories[i] != NULL; i++) {
		for (child = current->first_child; child != NULL; child = child->next) {
			if (child->type != MODEL_ENTRY_CATEGORY)
				continue;

			if (!strcmp (MIME_CATEGORY_INFO (child)->name, categories[i]))
				break;
		}

		if (child == NULL) {
			child = MODEL_ENTRY (mime_category_info_new (MIME_CATEGORY_INFO (current), categories[i], model));
			model_entry_insert_child (MODEL_ENTRY (child)->parent, MODEL_ENTRY (child), model);
		}

		current = child;
	}

	g_strfreev (categories);

	return MIME_CATEGORY_INFO (current);
}
