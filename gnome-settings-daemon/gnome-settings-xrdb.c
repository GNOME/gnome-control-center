/* -*- Mode: C; indent-tabs-mode: t; tab-width: 8; c-basic-offset: 8; style: linux -*- */

/* gnome-settings-xrdb.c
 *
 * Copyright © 2003 Ross Burton
 *
 * Written by Ross Burton <ross@burtonini.com>
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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <libgnome/gnome-i18n.h>
#include <gconf/gconf-client.h>

#include "gnome-settings-daemon.h"
#include "gnome-settings-xrdb.h"

#define SYSTEM_AD_DIR DATADIR "/control-center-2.0/xrdb"
#define GENERAL_AD SYSTEM_AD_DIR "/General.ad"
#define USER_AD_DIR ".gnome2/xrdb"
#define USER_X_RESOURCES ".Xresources"

#define GTK_THEME_KEY "/desktop/gnome/interface/gtk_theme"

GtkWidget *widget;

/*
 * Theme colour functions
 */

/*
 * TODO: replace with the code from Smooth? (which should really be
 * public in GTK+)
 */
static GdkColor*
colour_shade (GdkColor *a, gdouble shade, GdkColor *b)
{
	guint16 red, green, blue;
	
	red = CLAMP ((a->red) * shade, 0, 0xFFFF);
	green = CLAMP ((a->green) * shade, 0, 0xFFFF);
	blue = CLAMP ((a->blue) * shade, 0, 0xFFFF);
	
	b->red = red;
	b->green = green;
	b->blue = blue;
	
	return b;
}

static void
append_colour_define (GString *string, const gchar* name, const GdkColor* colour)
{
	g_return_if_fail (string != NULL);
	g_return_if_fail (name != NULL);
	g_return_if_fail (colour != NULL);

	g_string_append_printf (string, "#define %s #%2.2hx%2.2hx%2.2hx\n",
			       name, colour->red>>8, colour->green>>8, colour->blue>>8);
}

static void
append_theme_colours (GtkStyle *style, GString *string)
{
	GdkColor tmp;

	g_return_if_fail (style != NULL);
	g_return_if_fail (string != NULL);

	append_colour_define(string, "BACKGROUND",
			    &style->bg[GTK_STATE_NORMAL]);
	append_colour_define(string, "FOREGROUND",
			    &style->fg[GTK_STATE_NORMAL]);
	append_colour_define(string, "SELECT_BACKGROUND",
			    &style->bg[GTK_STATE_SELECTED]);
	append_colour_define(string, "SELECT_FOREGROUND",
			    &style->text[GTK_STATE_SELECTED]);
	append_colour_define(string, "WINDOW_BACKGROUND",
			    &style->base[GTK_STATE_NORMAL]);
	append_colour_define(string, "WINDOW_FOREGROUND",
			    &style->text[GTK_STATE_NORMAL]);
  
	append_colour_define(string, "INACTIVE_BACKGROUND",
			    &style->bg[GTK_STATE_INSENSITIVE]);
	append_colour_define(string, "INACTIVE_FOREGROUND",
			    &style->text[GTK_STATE_INSENSITIVE]);
	
	append_colour_define(string, "ACTIVE_BACKGROUND",
			    &style->bg[GTK_STATE_SELECTED]);
	append_colour_define(string, "ACTIVE_FOREGROUND",
			    &style->text[GTK_STATE_SELECTED]);

	append_colour_define(string, "HIGHLIGHT",
			     colour_shade (&style->bg[GTK_STATE_NORMAL],
					   1.2, &tmp));
	append_colour_define(string, "LOWLIGHT",
			     colour_shade (&style->bg[GTK_STATE_NORMAL],
					   2.0/3.0, &tmp));
	return;
}

/*
 * Directory scanning functions
 */

/**
 * Compare two file names on their base names.
 */
static gint
compare_basenames (gconstpointer a, gconstpointer b)
{
  char *base_a, *base_b;
  int res;
  base_a = g_path_get_basename (a);
  base_b = g_path_get_basename (b);
  res = strcmp (base_a, base_b);
  g_free (base_a);
  g_free (base_b);
  return res;
}

/**
 * Append the contents of a file onto the end of a GString
 */
static void
append_file (const char* file, GString *string, GError **error)
{
	gchar *contents;

	g_return_if_fail (string != NULL);
	g_return_if_fail (file != NULL);

	if (g_file_get_contents (file, &contents, NULL, error)) {
		g_string_append (string, contents);
		g_free (contents);
	}
}

/**
 * Scan a single directory for .ad files, and return them all in a
 * GSList*
 */
static GSList*
scan_ad_directory (const char *path, GError **error)
{
	GSList *list = NULL;
	GDir *dir;
	const gchar *entry;

	g_return_val_if_fail (path != NULL, NULL);
	dir = g_dir_open (path, 0, error);
	if (*error) {
		return NULL;
	}
	while ((entry = g_dir_read_name (dir)) != NULL) {
		int len;
		len = strlen (entry);
		if (g_str_has_suffix (entry, ".ad")) {
			list = g_slist_prepend (list, g_strdup_printf ("%s/%s", path, entry));
		}
	}
	g_dir_close (dir);
	/* TODO: sort still? */
	return g_slist_sort (list, (GCompareFunc)strcmp);
}

/**
 * Scan the user and system paths, and return a list of strings in the
 * right order for processing.
 */
static GSList*
scan_for_files (GError **error)
{
	const char* home_dir;
	
	GSList *user_list = NULL, *system_list = NULL;
	GSList *list = NULL, *p;

	system_list = scan_ad_directory (SYSTEM_AD_DIR, error);
	if (*error) return NULL;
	
	home_dir = g_get_home_dir ();
	if (home_dir) {
		char *user_ad = g_build_filename (home_dir, USER_AD_DIR, NULL);
		if (g_file_test (user_ad, G_FILE_TEST_IS_DIR)) {
			user_list = scan_ad_directory (user_ad, error);
			if (*error) {
				g_slist_foreach (system_list, (GFunc)g_free, NULL);
				g_slist_free (system_list);
				g_free (user_ad);
				return NULL;
			}
		}
		g_free (user_ad);
	} else {
		g_warning (_("Cannot determine user's home directory"));
	}

	/* An alternative approach would be to strdup() the strings
	   and free the entire contents of these lists, but that is a
	   little inefficient for my liking - RB */	
	for (p = system_list; p != NULL; p = g_slist_next (p)) {
		if (strcmp (p->data, GENERAL_AD) == 0) {
			/* We ignore this, free the data now */
			g_free (p->data);
			continue;
		}
		if (g_slist_find_custom (user_list, p->data, compare_basenames)) {
			/* Ditto */
			g_free (p->data);
			continue;
		}
		list = g_slist_prepend (list, p->data);
	}
	g_slist_free (system_list);

	for (p = user_list; p != NULL; p = g_slist_next (p)) {
		list = g_slist_prepend (list, p->data);
	}
	g_slist_free (user_list);

	/* Reverse the order so it is the correct way */
	list = g_slist_reverse (list);

	/* Add the initial file */
	list = g_slist_prepend (list, g_strdup (GENERAL_AD));

	return list;
}

/**
 * Append the users .Xresources file if it exists
 */
static void
append_xresources (GString *string, GError **error)
{
	const char* home_path;

	g_return_if_fail (string != NULL);

	home_path = g_get_home_dir ();
	if (home_path == NULL) {
		g_warning (_("Cannot determine user's home directory"));
		return;
	}
	char *xresources = g_build_filename (home_path, USER_X_RESOURCES, NULL);
	if (g_file_test (xresources, G_FILE_TEST_EXISTS)) {
		append_file (xresources, string, error);
		if (*error) {
			g_warning ("%s", (*error)->message);
			g_error_free (*error);
			error = NULL;
		}
	}
	g_free (xresources);
}

/*
 * Entry point
 */
static void
apply_settings (GtkStyle *style)
{
	char *xrdb[] = { "xrdb", "-merge", NULL };
	GString *string;
	GSList *list = NULL, *p;
	GError *error = NULL;

	string = g_string_sized_new (256);
	append_theme_colours (style, string);

	list = scan_for_files (&error);
	if (error) {
		g_warning (error->message);
		g_error_free (error);
		error = NULL;
	}
	for (p = list; p != NULL; p=g_slist_next (p)) {
		append_file (p->data, string, &error);
		if (error) {
			g_warning (error->message);
			g_error_free (error);
			error = NULL;
		}
	}
	
	g_slist_foreach (list, (GFunc)g_free, NULL);
	g_slist_free (list);
	
	append_xresources (string, &error);
	if (error) {
		g_warning (error->message);
		g_error_free (error);
		error = NULL;
	}

	gnome_settings_daemon_spawn_with_input (xrdb, string->str);
	g_string_free (string, TRUE);
	return;
}

static void
style_set_cb (GtkWidget *widget, GtkStyle *s, gpointer data)
{
	apply_settings (gtk_widget_get_style (widget));
}

void
gnome_settings_xrdb_init (GConfClient *client)
{
	widget = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_signal_connect (widget, "style-set", (GCallback)style_set_cb, NULL);
	gtk_widget_ensure_style (widget);
}

void
gnome_settings_xrdb_load (GConfClient *client)
{
	style_set_cb (widget, NULL, NULL);
}
