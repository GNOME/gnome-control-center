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

#include "mime-type-info.h"

static GList *dirty_list = NULL;

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

MimeTypeInfo *
mime_type_info_load (const gchar *mime_type)
{
	MimeTypeInfo            *info;
	Bonobo_ServerInfo       *component_info;

	info = g_new0 (MimeTypeInfo, 1);
	info->mime_type       = g_strdup (mime_type);
	info->description     = g_strdup (gnome_vfs_mime_get_description (mime_type));
	info->icon_name       = g_strdup (gnome_vfs_mime_get_icon (mime_type));
	info->file_extensions = gnome_vfs_mime_get_extensions_list (mime_type);
	info->edit_line       = g_strdup (gnome_vfs_mime_get_value (mime_type, "edit-line"));
	info->print_line      = g_strdup (gnome_vfs_mime_get_value (mime_type, "print-line"));
	info->default_action  = gnome_vfs_mime_get_default_application (mime_type);
	info->custom_line     = g_strdup (gnome_vfs_mime_get_value (mime_type, "custom-line"));

	component_info = gnome_vfs_mime_get_default_component (mime_type);

	if (component_info != NULL) {
		info->default_component_id = component_info->iid;
		CORBA_free (component_info);
	}

	return info;
}

void
mime_type_info_save (const MimeTypeInfo *info)
{
	gchar *tmp;
	gchar **array;
	GList *l;
	gint i = 0;

	gnome_vfs_mime_set_description (info->mime_type, info->description);
	gnome_vfs_mime_set_icon (info->mime_type, info->icon_name);
	gnome_vfs_mime_set_default_application (info->mime_type, info->default_action->id);
	gnome_vfs_mime_set_value (info->mime_type, "custom-line", info->custom_line);
	gnome_vfs_mime_set_value (info->mime_type, "print-line", info->print_line);
	gnome_vfs_mime_set_value (info->mime_type, "edit-line", info->edit_line);

	array = g_new0 (gchar *, g_list_length (info->file_extensions) + 1);
	for (l = info->file_extensions; l != NULL; l = l->next)
		array[i++] = l->data;
	tmp = g_strjoinv (" ", array);
	g_free (array);

	gnome_vfs_mime_set_extensions_list (info->mime_type, tmp);
	g_free (tmp);
}

void
mime_type_info_free (MimeTypeInfo *info)
{
	g_free (info->mime_type);
	g_free (info->description);
	g_free (info->icon_name);
	gnome_vfs_mime_extensions_list_free (info->file_extensions);
	g_free (info->default_component_id);
	gnome_vfs_mime_application_free (info->default_action);
	g_free (info->custom_line);
	g_free (info->edit_line);
	g_free (info->print_line);
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

