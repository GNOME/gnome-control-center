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

#include "libuuid/uuid.h"

#include "mime-type-info.h"
#include "mime-types-model.h"

/* List of MimeTypeInfo structures that have data to be committed */

static GList *dirty_list = NULL;

static const gchar *get_category_name (const gchar        *mime_type);
static GSList *get_lang_list          (void);
static gchar  *form_extensions_string (const MimeTypeInfo *info,
				       gchar              *sep,
				       gchar              *prepend);
static void    get_icon_pixbuf        (MimeTypeInfo       *info,
				       const gchar        *icon_path,
				       gboolean            want_large);

static MimeTypeInfo *get_category     (const gchar        *category_name);



void
load_all_mime_types (void) 
{
	GList *list, *tmp;

	list = gnome_vfs_get_registered_mime_types ();

	for (tmp = list; tmp != NULL; tmp = tmp->next)
		mime_type_info_new (tmp->data);

	g_list_free (list);
}

MimeTypeInfo *
mime_type_info_new (const gchar *mime_type)
{
	MimeTypeInfo      *info;

	info = g_new0 (MimeTypeInfo, 1);
	MODEL_ENTRY (info)->type = MODEL_ENTRY_MIME_TYPE;

	if (mime_type != NULL) {
		info->mime_type = g_strdup (mime_type);

		mime_type_info_set_category_name (info, get_category_name (mime_type));
	}

	return info;
}

MimeTypeInfo *
mime_type_info_new_category (MimeTypeInfo *parent, const gchar *category) 
{
	MimeTypeInfo      *info;

	info = g_new0 (MimeTypeInfo, 1);
	MODEL_ENTRY (info)->type = MODEL_ENTRY_CATEGORY;

	info->description = g_strdup (category);
	MODEL_ENTRY (info)->parent = MODEL_ENTRY (parent);

	if (parent != NULL)
		model_entry_insert_child (MODEL_ENTRY (parent), MODEL_ENTRY (info));

	return info;
}

/* Fill in the remaining fields in a MimeTypeInfo structure; suitable for
 * subsequent use in an edit dialog */

void
mime_type_info_load_all (MimeTypeInfo *info)
{
	gchar *tmp;

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
	GString *string;
	ModelEntry *tmp;
	gchar *ret;

	string = g_string_new ("");

	if (info->entry.type == MODEL_ENTRY_CATEGORY)
		tmp = MODEL_ENTRY (info);
	else
		tmp = info->entry.parent;

	for (; tmp->type != MODEL_ENTRY_NONE; tmp = tmp->parent) {
		g_string_prepend (string, MIME_TYPE_INFO (tmp)->description);
		g_string_prepend (string, "/");
	}

	ret = g_strdup ((*string->str == '\0') ? string->str : string->str + 1);
	g_string_free (string, TRUE);
	return ret;
}

GList *
mime_type_info_category_find_supported_apps (MimeTypeInfo *info)
{
	return NULL;
}

void
mime_type_info_set_category_name (const MimeTypeInfo *info, const gchar *category_name) 
{
	if (MODEL_ENTRY (info)->parent != NULL)
		model_entry_remove_child (MODEL_ENTRY (info)->parent, MODEL_ENTRY (info));

	if (category_name != NULL) {
		MODEL_ENTRY (info)->parent = MODEL_ENTRY (get_category (category_name));

		if (MODEL_ENTRY (info)->parent != NULL)
			model_entry_insert_child (MODEL_ENTRY (info)->parent, MODEL_ENTRY (info));
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
}

void
mime_type_info_free (MimeTypeInfo *info)
{
	dirty_list = g_list_remove (dirty_list, info);

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

void
mime_type_append_to_dirty_list (MimeTypeInfo *info)
{
	if (g_list_find (dirty_list, info) == NULL)
		dirty_list = g_list_prepend (dirty_list, info);
}

void
mime_type_remove_from_dirty_list (const gchar *mime_type)
{
	GList *tmp = dirty_list, *tmp1;

	while (tmp != NULL) {
		tmp1 = tmp->next;
		if (!strcmp (mime_type, ((MimeTypeInfo *) tmp->data)->mime_type))
			dirty_list = g_list_remove_link (dirty_list, tmp);
		tmp = tmp1;
	}
}

void
mime_type_commit_dirty_list (void)
{
	gnome_vfs_mime_freeze ();
	g_list_foreach (dirty_list, (GFunc) mime_type_info_save, NULL);
	gnome_vfs_mime_thaw ();
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

static MimeTypeInfo *
get_category (const gchar *category_name)
{
	ModelEntry *current, *child;
	gchar **categories;
	int i;

	if (category_name == NULL)
		return NULL;

	categories = g_strsplit (category_name, "/", -1);

	current = get_model_entries ();

	for (i = 0; categories[i] != NULL; i++) {
		for (child = current->first_child; child != NULL; child = child->next) {
			if (child->type != MODEL_ENTRY_CATEGORY)
				continue;

			if (!strcmp (MIME_TYPE_INFO (child)->description, categories[i]))
				break;
		}

		if (child == NULL)
			child = MODEL_ENTRY (mime_type_info_new_category (MIME_TYPE_INFO (current), categories[i]));

		current = child;
	}

	g_strfreev (categories);

	return MIME_TYPE_INFO (current);
}
