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
#   include "config.h"
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

static gint       xml_read_int           (xmlNodePtr node);
static xmlNodePtr xml_write_int          (gchar *name, 
					  gint number);
static gboolean   xml_read_bool          (xmlNodePtr node);
static xmlNodePtr xml_write_bool         (gchar *name,
					  gboolean value);

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
	gchar *value, *tmp;

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

	if (value) {
		tmp = g_strdup (value);
		prefs->screensavers = parse_screensaver_list (tmp);
		g_free (tmp);
	}
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

	/* Load default values */
	preferences_load_from_xrdb (prefs);

	prefs->selection_mode   = 3;
	prefs->power_management = FALSE;
	prefs->standby_time     = 0;
	prefs->suspend_time     = 0;
	prefs->power_down_time  = 20;

	return prefs;
}

static gint
clone_cb (gchar *key, gchar *value, Preferences *new_prefs) 
{
	g_tree_insert (new_prefs->config_db, key, g_strdup (value));
	return 0;
}

Preferences *
preferences_clone (Preferences *prefs) 
{
	Preferences *new_prefs;

	new_prefs = g_new0 (Preferences, 1);

	new_prefs->config_db = g_tree_new ((GCompareFunc) strcmp);

	new_prefs->selection_mode = prefs->selection_mode;
	new_prefs->power_management = prefs->power_management;
	new_prefs->standby_time = prefs->standby_time;
	new_prefs->suspend_time = prefs->suspend_time;
	new_prefs->power_down_time = prefs->power_down_time;

	g_tree_traverse (prefs->config_db, (GTraverseFunc) clone_cb,
			 G_IN_ORDER, new_prefs);

	read_prefs_from_db (new_prefs);
	return new_prefs;
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

static GList *
xml_get_programs_list (xmlNodePtr programs_node) 
{
	GList *list_head = NULL, *list_tail = NULL;
	xmlNodePtr node;
	Screensaver *saver;
	gint id = 0;

	for (node = programs_node->childs; node; node = node->next) {
		saver = screensaver_read_xml (node);
		if (!saver) continue;
		saver->id = id++;
		list_tail = g_list_append (list_tail, saver);
		if (list_head)
			list_tail = list_tail->next;
		else
			list_head = list_tail;
	}

	return list_head;
}

Preferences *
preferences_read_xml (xmlDocPtr xml_doc) 
{
	Preferences *prefs;
	xmlNodePtr root_node, node;

	prefs = preferences_new ();

	root_node = xmlDocGetRootElement (xml_doc);

	if (strcmp (root_node->name, "screensaver-prefs"))
		return NULL;

	for (node = root_node->childs; node; node = node->next) {
		if (!strcmp (node->name, "verbose"))
			prefs->verbose = xml_read_bool (node);
		else if (!strcmp (node->name, "lock"))
			prefs->lock = xml_read_bool (node);
		else if (!strcmp (node->name, "fade"))
			prefs->fade = xml_read_bool (node);
		else if (!strcmp (node->name, "unfade"))
			prefs->unfade = xml_read_bool (node);
		else if (!strcmp (node->name, "fade-seconds"))
			prefs->fade_seconds = xml_read_int (node);
		else if (!strcmp (node->name, "fade-ticks"))
			prefs->fade_ticks = xml_read_int (node);
		else if (!strcmp (node->name, "install-colormap"))
			prefs->install_colormap = xml_read_bool (node);
		else if (!strcmp (node->name, "nice"))
			prefs->nice = xml_read_int (node);
		else if (!strcmp (node->name, "timeout"))
			prefs->timeout = xml_read_int (node);
		else if (!strcmp (node->name, "lock-timeout"))
			prefs->lock_timeout = xml_read_int (node);
		else if (!strcmp (node->name, "cycle"))
			prefs->cycle = xml_read_int (node);
		else if (!strcmp (node->name, "programs"))
			prefs->screensavers = xml_get_programs_list (node);
		else if (!strcmp (node->name, "selection-mode"))
			prefs->selection_mode = xml_read_int (node);
		else if (!strcmp (node->name, "use-dpms"))
			prefs->power_management = xml_read_bool (node);
		else if (!strcmp (node->name, "standby-time"))
			prefs->standby_time = xml_read_int (node);
		else if (!strcmp (node->name, "suspend-time"))
			prefs->suspend_time = xml_read_int (node);
		else if (!strcmp (node->name, "shutdown-time"))
			prefs->power_down_time = xml_read_int (node);
	}

	return prefs;
}

static xmlNodePtr
xml_write_programs_list (GList *screensavers) 
{
	xmlNodePtr node;

	node = xmlNewNode (NULL, "programs");

	for (; screensavers; screensavers = screensavers->next)
		xmlAddChild (node, screensaver_write_xml 
			     (SCREENSAVER (screensavers->data)));

	return node;
}

xmlDocPtr 
preferences_write_xml (Preferences *prefs) 
{
	xmlDocPtr doc;
	xmlNodePtr node;

	doc = xmlNewDoc ("1.0");

	node = xmlNewDocNode (doc, NULL, "screensaver-prefs", NULL);

	xmlAddChild (node, xml_write_bool ("verbose", prefs->verbose));
	xmlAddChild (node, xml_write_bool ("lock", prefs->lock));
	xmlAddChild (node, xml_write_bool ("fade", prefs->fade));
	xmlAddChild (node, xml_write_bool ("unfade", prefs->unfade));
	xmlAddChild (node, xml_write_int ("fade-seconds", prefs->fade_seconds));
	xmlAddChild (node, xml_write_int ("fade-ticks", prefs->fade_ticks));
	xmlAddChild (node, xml_write_bool ("install-colormap",
					   prefs->install_colormap));
	xmlAddChild (node, xml_write_int ("nice", prefs->nice));
	xmlAddChild (node, xml_write_int ("timeout", prefs->timeout));
	xmlAddChild (node, xml_write_int ("lock-timeout", prefs->lock_timeout));
	xmlAddChild (node, xml_write_int ("cycle", prefs->cycle));
	xmlAddChild (node, xml_write_programs_list (prefs->screensavers));
	xmlAddChild (node, xml_write_int ("selection-mode",
					  prefs->selection_mode));
	xmlAddChild (node, xml_write_bool ("use-dpms",
					   prefs->power_management));
	xmlAddChild (node, xml_write_int ("standby-time", prefs->standby_time));
	xmlAddChild (node, xml_write_int ("suspend-time", prefs->suspend_time));
	xmlAddChild (node, xml_write_int ("shutdown-time",
					  prefs->power_down_time));

	xmlDocSetRootElement (doc, node);

	return doc;
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

Screensaver *
screensaver_read_xml (xmlNodePtr saver_node) 
{
	Screensaver *saver;
	xmlNodePtr node;

	if (strcmp (saver_node->name, "screensaver"))
		return NULL;

	saver = screensaver_new ();
	saver->enabled = FALSE;

	saver->label = g_strdup (xmlGetProp (saver_node, "_label"));

	for (node = saver_node->childs; node; node = node->next) {
		if (!strcmp (node->name, "name"))
			saver->name = g_strdup (xmlNodeGetContent (node));
		else if (!strcmp (node->name, "command-line"))
			saver->command_line =
				g_strdup (xmlNodeGetContent (node));
		else if (!strcmp (node->name, "visual"))
			saver->visual = g_strdup (xmlNodeGetContent (node));
		else if (!strcmp (node->name, "enabled"))
			saver->enabled = xml_read_bool (node);
	}

	return saver;
}

xmlNodePtr 
screensaver_write_xml (Screensaver *saver) 
{
	xmlNodePtr saver_node;

	saver_node = xmlNewNode (NULL, "screensaver");

	xmlNewProp (saver_node, "_label", saver->label);
	xmlNewChild (saver_node, NULL, "name", saver->name);
	xmlNewChild (saver_node, NULL, "command-line", saver->command_line);
	xmlNewChild (saver_node, NULL, "visual", saver->visual);
	xmlAddChild (saver_node, xml_write_bool ("enabled", saver->enabled));

	return saver_node;
}

char *
screensaver_get_desc (Screensaver *saver) 
{
	g_return_val_if_fail (saver != NULL, NULL);

	if (!saver->description)
		screensaver_get_desc_from_xrdb (saver);

	if (!saver->description)
		saver->description = g_strdup 
			(_("Custom screensaver. No description available"));

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


/* Read a numeric value from a node */

static gint
xml_read_int (xmlNodePtr node) 
{
	char *text;

	text = xmlNodeGetContent (node);

	if (text == NULL) 
		return 0;
	else
		return atoi (text);
}

/* Write out a numeric value in a node */

static xmlNodePtr
xml_write_int (gchar *name, gint number) 
{
	xmlNodePtr node;
	gchar *str;

	g_return_val_if_fail (name != NULL, NULL);

	str = g_strdup_printf ("%d", number);
	node = xmlNewNode (NULL, name);
	xmlNodeSetContent (node, str);
	g_free (str);

	return node;
}

/* Read a boolean value from a node */

static gboolean
xml_read_bool (xmlNodePtr node) 
{
	char *text;

	text = xmlNodeGetContent (node);

	if (!g_strcasecmp (text, "true")) 
		return TRUE;
	else
		return FALSE;
}

/* Write out a boolean value in a node */

static xmlNodePtr
xml_write_bool (gchar *name, gboolean value) 
{
	xmlNodePtr node;

	g_return_val_if_fail (name != NULL, NULL);

	node = xmlNewNode (NULL, name);

	if (value)
		xmlNodeSetContent (node, "true");
	else
		xmlNodeSetContent (node, "false");

	return node;
}
