/* -*- mode: c; style: linux -*- */

/* preferences.c
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen <hovinen@helixcode.com>
 * Parts written by Jamie Zawinski <jwz@jwz.org>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <gnome.h>

#include "preferences.h"
#include "preview.h"
#include "pref-file.h"
#include "resources.h"
#include "rc-parse.h"

static void
remove_entry (GTree *config_db, gchar *entry) 
{
	gchar *value_str;

	value_str = g_tree_lookup (config_db, "verbose");
	g_tree_remove (config_db, "verbose");
	if (value_str) g_free (value_str);
}

static void
read_prefs_from_db (Preferences *prefs) 
{
	gchar *value;

	value = g_tree_lookup (prefs->config_db, "verbose");
	if (value) prefs->verbose = parse_boolean_resource (value);

	value = g_tree_lookup (prefs->config_db, "lock");
	if (value) prefs->lock = parse_boolean_resource (value);

	value = g_tree_lookup (prefs->config_db, "fade");
	if (value) prefs->fade = parse_boolean_resource (value);

	value = g_tree_lookup (prefs->config_db, "unfade");
	if (value) prefs->unfade = parse_boolean_resource (value);

	value = g_tree_lookup (prefs->config_db, "fadeSeconds");
	if (value) prefs->fade_seconds = parse_seconds_resource (value);

	value = g_tree_lookup (prefs->config_db, "fadeTicks");
	if (value) prefs->fade_ticks = parse_integer_resource (value);

	value = g_tree_lookup (prefs->config_db, "installColormap");
	if (value) prefs->install_colormap = parse_boolean_resource (value);

	value = g_tree_lookup (prefs->config_db, "nice");
	if (value) prefs->nice = parse_integer_resource (value);

	value = g_tree_lookup (prefs->config_db, "timeout");
	if (value) prefs->timeout = parse_minutes_resource (value);

	value = g_tree_lookup (prefs->config_db, "lockTimeout");
	if (value) prefs->lock_timeout = parse_minutes_resource (value);

	value = g_tree_lookup (prefs->config_db, "cycle");
	if (value) prefs->cycle = parse_minutes_resource (value);

	value = g_tree_lookup (prefs->config_db, "programs");
	if (value) prefs->screensavers = parse_screensaver_list (value);
}

static void
store_prefs_in_db (Preferences *prefs) 
{
	remove_entry (prefs->config_db, "verbose");
	g_tree_insert (prefs->config_db, "verbose",
		       write_boolean (prefs->verbose));

	remove_entry (prefs->config_db, "lock");
	g_tree_insert (prefs->config_db, "lock",
		       write_boolean (prefs->lock));

	remove_entry (prefs->config_db, "fade");
	g_tree_insert (prefs->config_db, "fade",
		       write_boolean (prefs->fade));

	remove_entry (prefs->config_db, "unfade");
	g_tree_insert (prefs->config_db, "unfade",
		       write_boolean (prefs->unfade));

	remove_entry (prefs->config_db, "fadeSeconds");
	g_tree_insert (prefs->config_db, "fadeSeconds",
		       write_seconds (prefs->fade_seconds));

	remove_entry (prefs->config_db, "fadeTicks");
	g_tree_insert (prefs->config_db, "fadeTicks",
		       write_integer (prefs->fade_ticks));

	remove_entry (prefs->config_db, "installColormap");
	g_tree_insert (prefs->config_db, "installColormap",
		       write_boolean (prefs->install_colormap));

	remove_entry (prefs->config_db, "nice");
	g_tree_insert (prefs->config_db, "nice",
		       write_integer (prefs->nice));

	remove_entry (prefs->config_db, "timeout");
	g_tree_insert (prefs->config_db, "timeout",
		       write_minutes (prefs->timeout));

	remove_entry (prefs->config_db, "lockTimeout");
	g_tree_insert (prefs->config_db, "lockTimeout",
		       write_minutes (prefs->lock_timeout));

	remove_entry (prefs->config_db, "cycle");
	g_tree_insert (prefs->config_db, "cycle",
		       write_minutes (prefs->cycle));

	remove_entry (prefs->config_db, "programs");
	g_tree_insert (prefs->config_db, "programs",
		       write_screensaver_list (prefs->screensavers));
}

Preferences *
preferences_new (void) 
{
	Preferences *prefs;

	prefs = g_new0 (Preferences, 1);

	return prefs;
}

void
preferences_destroy (Preferences *prefs) 
{
	GList *node;

	if (!prefs) return;

	/* Destroy screensavers */
	if (prefs->screensavers) {
		for (node = prefs->screensavers; node; node = node->next)
			screensaver_destroy (SCREENSAVER (node->data));

		g_list_free (prefs->screensavers);
	}

	if (prefs->programs_list) g_free (prefs->programs_list);
	g_free (prefs);
}

void
preferences_load (Preferences *prefs) 
{
	g_return_if_fail (prefs != NULL);

	if (!preferences_load_from_file (prefs))
		preferences_load_from_xrdb (prefs);

	read_prefs_from_db (prefs);

	prefs->selection_mode =
		gnome_config_get_int ("/Screensaver/Default/selection_mode=3");
	prefs->power_management = 
		gnome_config_get_bool ("/Screensaver/Default/use_dpms=FALSE");
	prefs->standby_time = 
		gnome_config_get_int ("/Screensaver/Default/standby_time=0");
	prefs->suspend_time = 
		gnome_config_get_int ("/Screensaver/Default/suspend_time=0");
	prefs->power_down_time = 
		gnome_config_get_int ("/Screensaver/Default/shutdown_time=20");
}

void 
preferences_save (Preferences *prefs) 
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (prefs->config_db != NULL);

	store_prefs_in_db (prefs);

	preferences_save_to_file (prefs);

	gnome_config_set_int ("/Screensaver/Default/selection_mode",
			      prefs->selection_mode);
	gnome_config_set_bool ("/Screensaver/Default/use_dpms",
			       prefs->power_management);
	gnome_config_set_int ("/Screensaver/Default/standby_time",
			      prefs->standby_time);
	gnome_config_set_int ("/Screensaver/Default/suspend_time",
			      prefs->suspend_time);
	gnome_config_set_int ("/Screensaver/Default/shutdown_time",
			      prefs->power_down_time);
	gnome_config_sync ();
}

Screensaver *
screensaver_new (void) 
{
	Screensaver *saver;

	saver = g_new0 (Screensaver, 1);
	saver->name = NULL;
	saver->enabled = TRUE;

	return saver;
}

void
screensaver_destroy (Screensaver *saver) 
{
	if (!saver) return;

	if (saver->visual) g_free (saver->visual);
	if (saver->name) g_free (saver->name);
	if (saver->command_line) g_free (saver->command_line);
	if (saver->label) g_free (saver->label);
	if (saver->description) g_free (saver->description);

	g_free (saver);
}

GList *
screensaver_add (Screensaver *saver, GList *screensavers) 
{
	GList *list, *node;

	g_return_val_if_fail (saver != NULL, NULL);

	list = g_list_append (screensavers, saver);
	saver->link = g_list_find (list, saver);

	for (node = list; node != saver->link; node = node->next)
		saver->id++;

	return list;
}

GList *
screensaver_remove (Screensaver *saver, GList *screensavers) 
{
	GList *node;

	g_return_val_if_fail (saver != NULL, NULL);
	g_return_val_if_fail (saver->link != NULL, NULL);
	g_return_val_if_fail (saver == saver->link->data, NULL);

	for (node = saver->link->next; node; node = node->next)
		SCREENSAVER (node->data)->id--;

	return g_list_remove_link (screensavers, saver->link);
}

char *
screensaver_get_desc (Screensaver *saver) 
{
	g_return_val_if_fail (saver != NULL, NULL);

	if (!saver->description)
		screensaver_get_desc_from_xrdb (saver);

	if (!saver->description)
		saver->description = g_strdup (_("Custom screensaver. No description available"));

	return saver->description;
}

/* Adapted from xscreensaver 3.24 driver/demo-Gtk.c line 944 ... */

char *
screensaver_get_label (gchar *name) 
{
	char *s, *label;

	label = screensaver_get_label_from_xrdb (name);
	if (label) return label;

	label = g_strdup (name);

	for (s = label; *s; s++)    /* if it has any capitals, return it */
		if (*s >= 'A' && *s <= 'Z')
			return s;

	if (label[0] >= 'a' && label[0] <= 'z')             /* else cap it */
		label[0] -= 'a'-'A';
	if (label[0] == 'X' && label[1] >= 'a' && label[1] <= 'z')
		label[1] -= 'a'-'A';

	return label;
}
