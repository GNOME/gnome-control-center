/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* capplet-dir.c
 * Copyright (C) 2000, 2001 Ximian, Inc.
 * Copyright (C) 1998 Red Hat Software, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>,
 *            Jonathan Blandford <jrb@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include "control-center-categories.h"

#include <gnome.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <string.h>
#include <glib.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <libgnomevfs/gnome-vfs.h>

static char *
find_icon (GnomeDesktopItem *dentry)
{
	char *icon_file = gnome_desktop_item_get_icon (dentry, NULL);
	if (!icon_file)
		icon_file = gnome_program_locate_file (NULL, GNOME_FILE_DOMAIN_PIXMAP,
						       "gnome-unknown.png", TRUE, NULL);
	return icon_file;
}


#if 0
#include "capplet-dir.h"
#include "capplet-dir-view.h"

CappletDirView *(*get_view_cb) (CappletDir *dir, CappletDirView *launcher);

/* nice global table for capplet lookup */
GHashTable *capplet_hash = NULL;

CappletDirEntry *
capplet_new (CappletDir *dir, gchar *desktop_path) 
{
	Capplet *capplet;
	CappletDirEntry *entry;
	GnomeDesktopItem *dentry;
	gchar *path, **vec;
	const gchar *execstr;

	g_return_val_if_fail (desktop_path != NULL, NULL);

	entry = g_hash_table_lookup (capplet_hash, desktop_path);
	if (entry) {
		return entry;
	}

	dentry = gnome_desktop_item_new_from_uri (desktop_path,
						  GNOME_DESKTOP_ITEM_TYPE_NULL,
						  NULL);
	if (dentry == NULL)
		return NULL;

	execstr = gnome_desktop_item_get_string (dentry,
			GNOME_DESKTOP_ITEM_EXEC);
	/* Perhaps use poptParseArgvString here */
	vec = g_strsplit (execstr, " ", 0);
	if (!(execstr && execstr[0]) || !(vec && (path = g_find_program_in_path (vec[0]))))
	{
		g_strfreev (vec);
		gnome_desktop_item_unref (dentry);
		return NULL;
	}
	g_free (path);

	capplet = g_new0 (Capplet, 1);
	capplet->launching = FALSE;

	entry = CAPPLET_DIR_ENTRY (capplet);

	entry->type = TYPE_CAPPLET;
	entry->entry = dentry;

	entry->label = g_strdup (gnome_desktop_item_get_localestring (dentry,
			GNOME_DESKTOP_ITEM_NAME));
	entry->icon = find_icon (, dentry);
	entry->pb = gdk_pixbuf_new_from_file (entry->icon, NULL);
	entry->uri = gnome_vfs_uri_new (desktop_path);
	entry->exec = vec;
	entry->dir = dir;

	g_hash_table_insert (capplet_hash, g_strdup (desktop_path), entry);

	return entry;
}

CappletDirEntry *
capplet_dir_new (CappletDir *dir, gchar *dir_path)
{
	CappletDir *capplet_dir;
	CappletDirEntry *entry;
	GnomeVFSURI *desktop_uri;
	GnomeVFSURI *dir_uri;
	char *desktop_uri_string;

	g_return_val_if_fail (dir_path != NULL, NULL);


	entry = g_hash_table_lookup (capplet_hash, dir_path);
	if (entry) {
		return entry;
	}

	desktop_uri = gnome_vfs_uri_append_file_name (dir_uri, ".directory");
	desktop_uri_string = gnome_vfs_uri_to_string (desktop_uri, GNOME_VFS_URI_HIDE_NONE);

	capplet_dir = g_new0 (CappletDir, 1);
	entry = CAPPLET_DIR_ENTRY (capplet_dir);

	entry->type = TYPE_CAPPLET_DIR;
	entry->entry = gnome_desktop_item_new_from_uri (desktop_uri_string,
			GNOME_DESKTOP_ITEM_TYPE_NULL,
			NULL);
	entry->dir = dir;
	entry->uri = dir_uri;

	gnome_vfs_uri_unref (desktop_uri);
	g_free (desktop_uri_string);

	if (entry->entry) {
		entry->label = g_strdup (gnome_desktop_item_get_localestring (
				entry->entry,
				GNOME_DESKTOP_ITEM_NAME));
		entry->icon = find_icon (gnome_desktop_item_get_string (entry->entry,
									GNOME_DESKTOP_ITEM_ICON),
					 entry->entry);

		if (!entry->icon)
			entry->icon = gnome_program_locate_file
				(gnome_program_get (), GNOME_FILE_DOMAIN_APP_PIXMAP,
				 "control-center2.png", TRUE, NULL);

		entry->pb = gdk_pixbuf_new_from_file (entry->icon, NULL);
	} else {
		/* If the .directory file could not be found or read, abort */
		g_free (capplet_dir);
		return NULL;
	}

	entry->dir = dir;

	g_hash_table_insert (capplet_hash, g_strdup (dir_path), entry);

	capplet_dir_load (CAPPLET_DIR (entry));

	return entry;
}

CappletDirEntry *
capplet_lookup (const char *path)
{
	return g_hash_table_lookup (capplet_hash, path);
}

void 
capplet_dir_entry_destroy (CappletDirEntry *entry)
{
	if (entry->type == TYPE_CAPPLET) {
		capplet_shutdown (CAPPLET (entry));
	} else {
		capplet_dir_shutdown (CAPPLET_DIR (entry));
	}

	g_free (entry->label);
	g_free (entry->icon);
	gnome_vfs_uri_unref (entry->uri);
	g_strfreev (entry->exec);
	if (entry->entry)
		gnome_desktop_item_unref (entry->entry);
	g_free (entry);
}

void 
capplet_dir_entry_activate (CappletDirEntry *entry, 
			    CappletDirView *launcher)
{
	g_return_if_fail (entry != NULL);

	if (entry->type == TYPE_CAPPLET)
		capplet_activate (CAPPLET (entry));
	else if (entry->type == TYPE_CAPPLET_DIR)
		capplet_dir_activate (CAPPLET_DIR (entry), launcher);
	else
		g_assert_not_reached ();
}

void 
capplet_dir_entry_shutdown (CappletDirEntry *entry)
{
	if (entry->type == TYPE_CAPPLET)
		capplet_shutdown (CAPPLET (entry));
	else if (entry->type == TYPE_CAPPLET_DIR)
		capplet_dir_shutdown (CAPPLET_DIR (entry));
	else
		g_assert_not_reached ();
}

static gint
capplet_reset_cb (Capplet *capplet) 
{
	capplet->launching = FALSE;
	return FALSE;
}

static void
capplet_activate (Capplet *capplet) 
{
	CappletDirEntry *entry;

	entry = CAPPLET_DIR_ENTRY (capplet);

	if (capplet->launching) {
		return;
	} else {
		capplet->launching = TRUE;
		gtk_timeout_add (1000, (GtkFunction) capplet_reset_cb, capplet);
		gnome_desktop_item_launch (entry->entry, NULL, 0, NULL);
	}
}

void
capplet_dir_load (CappletDir *capplet_dir) 
{
	if (capplet_dir->entries) return;
	capplet_dir->entries = read_entries (capplet_dir);
}

static void
capplet_dir_activate (CappletDir *capplet_dir, CappletDirView *launcher) 
{
	capplet_dir_load (capplet_dir);
	capplet_dir->view = get_view_cb (capplet_dir, launcher);

	capplet_dir_view_load_dir (capplet_dir->view, capplet_dir);
	capplet_dir_view_show (capplet_dir->view);
}

static void
capplet_shutdown (Capplet *capplet) 
{
	/* Can't do much here ... :-( */
}

static void
cde_destroy (CappletDirEntry *e, gpointer null)
{
	capplet_dir_entry_destroy (e);
}

static void
capplet_dir_shutdown (CappletDir *capplet_dir) 
{
	if (capplet_dir->view)
		g_object_unref (G_OBJECT (capplet_dir->view));

	g_slist_foreach (capplet_dir->entries, (GFunc) cde_destroy, NULL);

	g_slist_free (capplet_dir->entries);
}

static gint 
node_compare (gconstpointer a, gconstpointer b) 
{
	return strcmp (CAPPLET_DIR_ENTRY (a)->label, 
		       CAPPLET_DIR_ENTRY (b)->label);
}
#endif
/* Adapted from the original control center... */

GnomeDesktopItem *
get_directory_entry (GnomeVFSURI *uri)
{
	GnomeVFSURI *desktop_uri;
	char *desktop_uri_string;
	GnomeDesktopItem *entry;

	desktop_uri = gnome_vfs_uri_append_file_name (uri, ".directory");
	desktop_uri_string = gnome_vfs_uri_to_string (desktop_uri, GNOME_VFS_URI_HIDE_NONE);

	entry = gnome_desktop_item_new_from_uri (desktop_uri_string,
						 GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS,
						 NULL);
	gnome_vfs_uri_unref (desktop_uri);
	g_free (desktop_uri_string);
	return entry;
}

typedef void (*DirectoryCallback) (GnomeVFSURI *uri, const char *name, GnomeDesktopItem *entry, gpointer user_data);
typedef void (*EntryCallback) (GnomeVFSURI *uri, const char *name, GnomeDesktopItem *entry, gpointer user_data);

static void
read_entries (GnomeVFSURI *uri, gint auto_recurse_level, DirectoryCallback dcb, EntryCallback ecb, gpointer user_data)
{
	gchar *test;
	GnomeVFSDirectoryHandle *parent_dir;
	GnomeVFSResult result;

	GnomeVFSFileInfo *child = gnome_vfs_file_info_new ();

	result = gnome_vfs_directory_open_from_uri (&parent_dir, uri,
						    GNOME_VFS_FILE_INFO_DEFAULT);

	if (result != GNOME_VFS_OK)
		return;
	
	while (gnome_vfs_directory_read_next (parent_dir, child) == GNOME_VFS_OK) {
		GnomeVFSURI *fulluri;
		char *fullpath;

      	        if (child->name[0] == '.')
	                continue;

		fulluri = gnome_vfs_uri_append_path (uri, child->name);
		fullpath = gnome_vfs_uri_to_string (fulluri, GNOME_VFS_URI_HIDE_NONE);

		if (child->type == GNOME_VFS_FILE_TYPE_DIRECTORY) {
			if (auto_recurse_level != 0) {
				gint recurse;
				if (auto_recurse_level < 0)
					recurse = auto_recurse_level;
				else
					recurse = auto_recurse_level - 1;
				read_entries (fulluri, recurse, dcb, ecb, user_data);
			} else {
				GnomeDesktopItem *entry;

				entry = get_directory_entry (fulluri);
				dcb (fulluri, child->name, entry, user_data);
				if (entry)
					gnome_desktop_item_unref (entry);
			}
		} else {
		  
			test = rindex(child->name, '.');
		  
			/* if it's a .desktop file, it's interesting for sure! */
			if (test && !strcmp (".desktop", test)) {
				GnomeDesktopItem *entry;
				entry = gnome_desktop_item_new_from_uri (fullpath,
									 GNOME_DESKTOP_ITEM_LOAD_ONLY_IF_EXISTS,
									 NULL);
				if (entry) {
					ecb (fulluri, child->name, entry, user_data);
					gnome_desktop_item_unref (entry);
				}
			}
		}
		gnome_vfs_uri_unref (fulluri);
		g_free (fullpath);
        }
        
	gnome_vfs_directory_close (parent_dir);
}

typedef struct {
	GSList *entries;
	GSList *names;
	GnomeDesktopItem *directory_entry;
	char *name;
} CategoryLoadInformation;

typedef struct {
	CategoryLoadInformation *other;
	GSList *categories;
} FullLoadInformation;

static void
do_sort (GnomeDesktopItem *item, void **base, size_t nmemb,
	 int (*compar)(const void *, const void *),
	 const char *(*get_name)(const void *))
{
	char **sort_orders = NULL;
	qsort (base, nmemb, sizeof (void *), compar);
	if (item)
		sort_orders = gnome_desktop_item_get_strings (item, GNOME_DESKTOP_ITEM_SORT_ORDER);
	if (sort_orders) {
		int i;
		int j = 0;
		for (i = 0; sort_orders[i]; i++) {
			int k;
			for (k = j; k < nmemb; k++) {
				const char *name = get_name (base[k]);
				if (name && !strcmp (name, sort_orders[i])) {
					void *temp = base[k];
					memmove (base + j + 1, base + j, (k - j) * sizeof (void *));
					base[j] = temp;
					j++;
					/* if (j >= nmemb), then k >= j >= nmemb
					   and thus we don't need a break
					   here. */
				}
			}
			if (j >= nmemb)
				break;
		}
		g_strfreev (sort_orders);
	}
}


static int 
compare_entries (const void *ap, const void *bp) 
{
	ControlCenterEntry *a = *(ControlCenterEntry **)ap;
	ControlCenterEntry *b = *(ControlCenterEntry **)bp;
	if (a->title == NULL && b->title == NULL)
		return 0;
	if (a->title == NULL)
		return 1;
	if (b->title == NULL)
		return -1;
	return strcmp (a->title,
		       b->title);
}

static const char *
get_entry_name (const void *ap)
{
	ControlCenterEntry *a = (ControlCenterEntry *)ap;
	return a->name;
}

static void
free_entry (ControlCenterEntry *entry)
{
	g_free (entry->icon);
	g_free (entry->name);
	if (entry->desktop_entry)
		gnome_desktop_item_unref (entry->desktop_entry);
}

static void
free_category (ControlCenterCategory *category)
{
	int i;
	for (i = 0; i < category->count; i++) {
		free_entry (category->entries[i]);
	}
	g_free (category->entries);
	if (category->directory_entry)
		gnome_desktop_item_unref (category->directory_entry);
}

static ControlCenterCategory *
load_information_to_category (CategoryLoadInformation *info, gboolean real_category)
{
	ControlCenterCategory *category = g_new (ControlCenterCategory, 1);
	int i;
	GSList *iterator, *name_iterator;

	category->count = g_slist_length (info->entries);
	category->entries = g_new (ControlCenterEntry *, category->count);
	category->directory_entry = info->directory_entry;
	category->title = NULL;
	category->name = info->name;
	category->user_data = NULL;
	category->real_category = real_category;
	if (category->directory_entry)
		category->title = gnome_desktop_item_get_localestring (category->directory_entry,
								 GNOME_DESKTOP_ITEM_NAME);

	if (!category->title || !category->title[0])
		category->title = category->name;
	if (!category->title || !category->title[0])
		category->title = _("Others");

	for (i = 0, iterator = info->entries, name_iterator = info->names;
	     i < category->count;
	     i++, iterator = iterator->next, name_iterator = name_iterator->next) {
		category->entries[i] = g_new (ControlCenterEntry, 1);

		category->entries[i]->desktop_entry = iterator->data;
		category->entries[i]->icon = find_icon (category->entries[i]->desktop_entry);
		category->entries[i]->user_data = NULL;
		category->entries[i]->name = name_iterator->data;
		category->entries[i]->category = category;
		if (category->entries[i]->desktop_entry) {
			category->entries[i]->title =
				gnome_desktop_item_get_localestring (category->entries[i]->desktop_entry,
							       GNOME_DESKTOP_ITEM_NAME);
			category->entries[i]->comment =
				gnome_desktop_item_get_localestring (category->entries[i]->desktop_entry,
							       GNOME_DESKTOP_ITEM_COMMENT);
		} else {
			category->entries[i]->title = NULL;
			category->entries[i]->comment = NULL;
		}
	}

	do_sort (category->directory_entry, (void **) category->entries, category->count, compare_entries, get_entry_name);

	g_slist_free (info->entries);
	g_slist_free (info->names);

	return category;
}

static int 
compare_categories (const void *ap, const void *bp) 
{
	ControlCenterCategory *a = *(ControlCenterCategory **)ap;
	ControlCenterCategory *b = *(ControlCenterCategory **)bp;
	if (a->title == NULL && b->title == NULL)
		return 0;
	if (a->title == NULL)
		return 1;
	if (b->title == NULL)
		return -1;
	return strcmp (a->title,
		       b->title);
}

static const char *
get_category_name (const void *ap)
{
	ControlCenterCategory *a = (ControlCenterCategory *)ap;
	return a->name;
}

static ControlCenterInformation *
load_information_to_information (GnomeVFSURI *base_uri, FullLoadInformation *info)
{
	ControlCenterInformation *information = g_new (ControlCenterInformation, 1);
	int i;
	GSList *iterator;

	information->count = g_slist_length (info->categories);
	information->categories = g_new (ControlCenterCategory *, information->count);

	i = 0;
	for (iterator = info->categories; iterator != NULL; iterator = iterator->next) {
		ControlCenterCategory *category = iterator->data;
		if (category->count)
			information->categories[i++] = category;
		else {
			free_category (category);
		}
	}

	if (information->count != i) {
		information->count = i;
		information->categories = g_renew (ControlCenterCategory *, information->categories, information->count);
	}

	g_slist_free (info->categories);

	information->directory_entry = get_directory_entry (base_uri);
	information->title = NULL;
	if (information->directory_entry) {
		do_sort (information->directory_entry, (void **) information->categories, information->count, compare_categories, get_category_name);
		information->title = gnome_desktop_item_get_localestring (information->directory_entry,
								    GNOME_DESKTOP_ITEM_NAME);
	}

	if (!information->title || !information->title[0])
		information->title = _("Gnome Control Center");

	return information;
}

static void
add_to_category (GnomeVFSURI *uri, const char *name, GnomeDesktopItem *entry, gpointer user_data)
{
	CategoryLoadInformation *catinfo = user_data;

	catinfo->entries = g_slist_prepend (catinfo->entries, entry);
	catinfo->names = g_slist_prepend (catinfo->names, g_strdup (name));

	gnome_desktop_item_ref (entry);
}

static void
add_to_other (GnomeVFSURI *uri, const char *name, GnomeDesktopItem *entry, gpointer user_data)
{
	FullLoadInformation *info = user_data;
	if (info->other == NULL) {
		info->other = g_new (CategoryLoadInformation, 1);
		info->other->entries = NULL;
		info->other->names = NULL;
		info->other->directory_entry = NULL;
		info->other->name = NULL;
	}
	add_to_category (uri, name, entry, info->other);
}

static void
create_category (GnomeVFSURI *uri, const char *name, GnomeDesktopItem *entry, gpointer user_data)
{
	FullLoadInformation *info = user_data;
	CategoryLoadInformation catinfo;

	catinfo.entries = NULL;
	catinfo.names = NULL;
	catinfo.directory_entry = entry;
	catinfo.name = g_strdup (name);
	if (entry)
		gnome_desktop_item_ref (entry);

	read_entries (uri, -1, NULL, add_to_category, &catinfo);

	info->categories = g_slist_prepend (info->categories,
					    load_information_to_category (&catinfo, TRUE));
}

ControlCenterInformation *
control_center_get_categories (const gchar *prefsuri)
{
	FullLoadInformation info;
	GnomeVFSURI *uri;
	ControlCenterInformation *information;

	info.categories = NULL;
	info.other = NULL;

	uri = gnome_vfs_uri_new (prefsuri);
	read_entries (uri, 0, create_category, add_to_other, &info);

	if (info.other)
		info.categories = g_slist_prepend (info.categories, load_information_to_category (info.other, FALSE));
	g_free (info.other);

	information = load_information_to_information (uri, &info);
	gnome_vfs_uri_unref (uri);

	return information;
}
