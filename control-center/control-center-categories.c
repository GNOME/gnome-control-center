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

/********************************************************************
 *
 * Stolen from nautilus to keep control center and nautilus in sync
 */
static gboolean
eel_str_has_suffix (const char *haystack, const char *needle)
{
	const char *h, *n;

	if (needle == NULL) {
		return TRUE;
	}
	if (haystack == NULL) {
		return needle[0] == '\0';
	}
		
	/* Eat one character at a time. */
	h = haystack + strlen(haystack);
	n = needle + strlen(needle);
	do {
		if (n == needle) {
			return TRUE;
		}
		if (h == haystack) {
			return FALSE;
		}
	} while (*--h == *--n);
	return FALSE;
}
static char *   
eel_str_strip_trailing_str (const char *source, const char *remove_this)
{
	const char *end;
	if (source == NULL) {
		return NULL;
	}
	if (remove_this == NULL) {
		return g_strdup (source);
	}
	end = source + strlen (source);
	if (strcmp (end - strlen (remove_this), remove_this) != 0) {
		return g_strdup (source);
	}
	else {
		return g_strndup (source, strlen (source) - strlen(remove_this));
	}
	
}
static char *
nautilus_remove_icon_file_name_suffix (const char *icon_name)
{
	guint i;
	const char *suffix;
	static const char *icon_file_name_suffixes[] = { ".svg", ".svgz", ".png", ".jpg", ".xpm" };

	for (i = 0; i < G_N_ELEMENTS (icon_file_name_suffixes); i++) {
		suffix = icon_file_name_suffixes[i];
		if (eel_str_has_suffix (icon_name, suffix)) {
			return eel_str_strip_trailing_str (icon_name, suffix);
		}
	}
	return g_strdup (icon_name);
}
/********************************************************************/

static GdkPixbuf * 
find_icon (GnomeDesktopItem *dentry) 
{
	GdkPixbuf *res;
	char const *icon;
	GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();

	icon = gnome_desktop_item_get_string (dentry, GNOME_DESKTOP_ITEM_ICON);

	if (icon == NULL || icon[0] == 0)
		icon = "gnome-settings";
	else if (g_path_is_absolute (icon))
		res = gdk_pixbuf_new_from_file (icon, NULL);
	else  {
		char *no_suffix = nautilus_remove_icon_file_name_suffix (icon);
		res = gtk_icon_theme_load_icon (icon_theme, no_suffix, 48, 0, NULL);
		g_free (no_suffix);
		if (res == NULL) {
			char *path = g_build_filename (GNOMECC_ICONS_DIR, icon, NULL);
			res = gdk_pixbuf_new_from_file (path, NULL);
			g_free (path);
		}
	}
	if (res == NULL)
		res = gtk_icon_theme_load_icon (icon_theme, "gnome-unknown", 48, 0, NULL);
	if (res == NULL)
		res = gtk_icon_theme_load_icon (icon_theme, "gtk-missing-image", 48, 0, NULL);
	return res;
}

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
	g_object_unref (entry->icon_pixbuf);
	entry->icon_pixbuf = NULL;
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
		category->entries[i]->icon_pixbuf = find_icon (category->entries[i]->desktop_entry);
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
