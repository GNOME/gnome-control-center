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

#include "mime-type-info.h"
#include "mime-types-model.h"

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

MimeTypeInfo *
mime_type_info_load (GtkTreeModel *model, GtkTreeIter *iter)
{
	MimeTypeInfo      *info;
	Bonobo_ServerInfo *component_info;
	GValue             mime_type;

	mime_type.g_type = G_TYPE_INVALID;
	gtk_tree_model_get_value (model, iter, MIME_TYPE_COLUMN, &mime_type);

	info = g_new0 (MimeTypeInfo, 1);
	info->model           = model;
	info->iter            = gtk_tree_iter_copy (iter);
	info->mime_type       = g_value_dup_string (&mime_type);
	info->description     = g_strdup (gnome_vfs_mime_get_description (info->mime_type));
	info->icon_name       = g_strdup (gnome_vfs_mime_get_icon (info->mime_type));
	info->file_extensions = gnome_vfs_mime_get_extensions_list (info->mime_type);
	info->edit_line       = g_strdup (gnome_vfs_mime_get_value (info->mime_type, "edit-line"));
	info->print_line      = g_strdup (gnome_vfs_mime_get_value (info->mime_type, "print-line"));
	info->default_action  = gnome_vfs_mime_get_default_application (info->mime_type);

	if (info->default_action != NULL && !strncmp (info->default_action->id, "custom-", strlen ("custom-"))) {
		info->custom_line = g_strdup (info->default_action->command);
		gnome_vfs_mime_application_free (info->default_action);
		info->default_action = NULL;
	}

	component_info = gnome_vfs_mime_get_default_component (info->mime_type);

	if (component_info != NULL) {
		info->default_component_id = component_info->iid;
		CORBA_free (component_info);
	}

	g_value_unset (&mime_type);

	return info;
}

void
mime_type_info_save (const MimeTypeInfo *info)
{
	gchar *tmp, *tmp1;
	gchar *appid;

	gnome_vfs_mime_set_description (info->mime_type, info->description);
	gnome_vfs_mime_set_icon (info->mime_type, info->icon_name);
	gnome_vfs_mime_set_value (info->mime_type, "print-line", info->print_line);
	gnome_vfs_mime_set_value (info->mime_type, "edit-line", info->edit_line);

	if (info->default_action != NULL) {
		gnome_vfs_mime_set_default_application (info->mime_type, info->default_action->id);
	}
	else if (info->custom_line != NULL && *info->custom_line != '\0') {
		tmp = g_strdup (info->mime_type);
		for (tmp1 = tmp; *tmp1 != '\0'; tmp1++)
			if (*tmp1 == '/') *tmp1 = '-';
		appid = g_strconcat ("custom-", tmp, NULL);
		g_free (tmp);

		gnome_vfs_application_registry_set_value (appid, "command", info->custom_line);

		gnome_vfs_mime_set_default_application (info->mime_type, appid);
	}

	tmp = form_extensions_string (info, " ", NULL);
	gnome_vfs_mime_set_extensions_list (info->mime_type, tmp);
	g_free (tmp);
}

void
mime_type_info_update (MimeTypeInfo *info) 
{
	GdkPixbuf *pixbuf;
	gchar *tmp;

	pixbuf = get_icon_pixbuf (info->icon_name);

	tmp = form_extensions_string (info, ", ", ".");
	gtk_tree_store_set (GTK_TREE_STORE (info->model), info->iter,
			    DESCRIPTION_COLUMN, info->description,
			    ICON_COLUMN, pixbuf,
			    EXTENSIONS_COLUMN, tmp,
			    -1);
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

