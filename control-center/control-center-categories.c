/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Copyright (C) 2004 Red Hat, Inc.
 * Copyright (C) 2000, 2001 Ximian, Inc.
 * Copyright (C) 1998 Red Hat Software, Inc.
 *
 * Written by Mark McLoughlin <mark@skynet.ie>
 *            Bradford Hovinen <hovinen@ximian.com>,
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

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#define MENU_I_KNOW_THIS_IS_UNSTABLE
#include <menu-tree.h>

static char *
remove_icon_suffix (const char *icon)
{
	static const char *icon_suffixes[] = { ".svg", ".svgz", ".png", ".jpg", ".xpm" };
	int i;

	for (i = 0; i < G_N_ELEMENTS (icon_suffixes); i++)
		if (g_str_has_suffix (icon, icon_suffixes [i]))
			return g_strndup (icon, strlen (icon) - strlen (icon_suffixes [i]));

	return g_strdup (icon);
}

static GdkPixbuf * 
load_icon (const char *icon)
{
	GtkIconTheme *icon_theme;
	GdkPixbuf    *retval;

	icon_theme = gtk_icon_theme_get_default ();

	if (!g_path_is_absolute (icon)) {
		char *no_suffix;

		no_suffix = remove_icon_suffix (icon);
		retval = gtk_icon_theme_load_icon (icon_theme, no_suffix, 48, 0, NULL);
		g_free (no_suffix);

		if (!retval) {
			char *path;

			path = g_build_filename (GNOMECC_ICONS_DIR, icon, NULL);
			retval = gdk_pixbuf_new_from_file (path, NULL);
			g_free (path);
		}
	} else {
		retval = gdk_pixbuf_new_from_file (icon, NULL);
	}

	if (!retval)
		retval = gtk_icon_theme_load_icon (icon_theme, "gnome-settings", 48, 0, NULL);
	if (!retval)
		retval = gtk_icon_theme_load_icon (icon_theme, "gnome-unknown", 48, 0, NULL);
	if (!retval)
		retval = gtk_icon_theme_load_icon (icon_theme, "gtk-missing-image", 48, 0, NULL);

	return retval;
}

static ControlCenterEntry *
control_center_entry_new (ControlCenterCategory *category,
			  MenuTreeEntry         *menu_entry)
{
	ControlCenterEntry *retval;

	retval = g_new0 (ControlCenterEntry, 1);

	retval->category      = category;
	retval->title         = g_strdup (menu_tree_entry_get_name (menu_entry));
	retval->comment       = g_strdup (menu_tree_entry_get_comment (menu_entry));
	retval->desktop_entry = g_strdup (menu_tree_entry_get_desktop_file_path (menu_entry));
	retval->icon_pixbuf   = load_icon (menu_tree_entry_get_icon (menu_entry));

	return retval;
}

static void
control_center_entry_free (ControlCenterEntry *entry)
{
	if (entry->icon_pixbuf)
		g_object_unref (entry->icon_pixbuf);
	entry->icon_pixbuf = NULL;

	g_free (entry->desktop_entry);
	entry->desktop_entry = NULL;

	g_free (entry->comment);
	entry->comment = NULL;

	g_free (entry->title);
	entry->title = NULL;

	entry->category = NULL;

	g_free (entry);
}

static void
populate_category (ControlCenterCategory *category,
		   MenuTreeDirectory     *menu_directory)
{
	GSList *menu_entries;
	GSList *entries;
	GSList *l;

	entries = NULL;

	menu_entries = menu_tree_directory_get_entries (menu_directory);
	for (l = menu_entries; l; l = l->next) {
		MenuTreeEntry *menu_entry = l->data;

		entries = g_slist_prepend (entries,
					   control_center_entry_new (category, menu_entry));

		menu_tree_entry_unref (menu_entry);
	}

	g_slist_free (menu_entries);

	if (entries != NULL) {
		GSList *l;
		int     i;

		category->n_entries = g_slist_length (entries);
		category->entries   = g_new0 (ControlCenterEntry *, category->n_entries + 1);

		for (l = entries, i = 0; l; l = l->next, i++)
			category->entries [i] = l->data;

		g_slist_free (entries);
	}
}

static ControlCenterCategory *
control_center_category_new (MenuTreeDirectory *menu_directory,
			     const char        *title,
			     gboolean           real_category)
{
	ControlCenterCategory *retval;

	retval = g_new0 (ControlCenterCategory, 1);

	retval->title         = g_strdup (title ? title : menu_tree_directory_get_name (menu_directory));
	retval->real_category = real_category != FALSE;

	populate_category (retval, menu_directory);

	return retval;
}

static void
control_center_category_free (ControlCenterCategory *category)
{
	int i;

	for (i = 0; i < category->n_entries; i++) {
		control_center_entry_free (category->entries [i]);
		category->entries [i] = NULL;
	}

	g_free (category->entries);
	category->entries = NULL;

	g_free (category);
}

static GSList *
read_categories_from_menu_directory (MenuTreeDirectory *directory,
				     GSList            *categories)
{
	GSList *subdirs;
	GSList *l;

	subdirs = menu_tree_directory_get_subdirs (directory);
	for (l = subdirs; l; l = l->next) {
		MenuTreeDirectory *subdir = l->data;

		categories = g_slist_prepend (categories,
					      control_center_category_new (subdir, NULL, TRUE));

		categories = read_categories_from_menu_directory (subdir, categories);

		menu_tree_directory_unref (subdir);
	}

	g_slist_free (subdirs);

	return categories;
}

static int
compare_categories (ControlCenterCategory *a,
		    ControlCenterCategory *b)
{
	return strcmp (a->title, b->title);
}

ControlCenterInformation *
control_center_get_information (void)
{
	ControlCenterInformation *information;
	MenuTree                 *menu_tree;
	MenuTreeDirectory        *menu_root;
	GSList                   *categories;

	information = g_new0 (ControlCenterInformation, 1);

	menu_tree = menu_tree_lookup ("preferences.menu");

	if (!(menu_root = menu_tree_get_root_directory (menu_tree))) {
		menu_tree_unref (menu_tree);
		return information;
	}

	categories = read_categories_from_menu_directory (menu_root, NULL);
	categories = g_slist_sort (categories,
				   (GCompareFunc) compare_categories);

	categories = g_slist_append (categories,
				     control_center_category_new (menu_root, _("Others"), FALSE));

	menu_tree_directory_unref (menu_root);

	if (categories != NULL) {
		GSList *l;
		int     i;

		information->n_categories = g_slist_length (categories);
		information->categories   = g_new0 (ControlCenterCategory *, information->n_categories + 1);

		for (l = categories, i = 0; l; l = l->next, i++)
			information->categories [i] = l->data;

		g_slist_free (categories);
	}

	menu_tree_unref (menu_tree);

	return information;
}

void
control_center_information_free (ControlCenterInformation *information)
{
	int i;

	for (i = 0; i < information->n_categories; i++) {
		control_center_category_free (information->categories [i]);
		information->categories [i] = NULL;
	}

	information->n_categories = 0;

	g_free (information->categories);
	information->categories = NULL;

	g_free (information);
}
