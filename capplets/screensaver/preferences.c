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
#include <sys/types.h>
#include <dirent.h>

#include <gnome.h>
#include <libxml/parser.h>
#include <gconf/gconf-client.h>

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
		parse_screensaver_list (prefs->savers_hash, tmp);
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

static GList*
screensaver_list_prepend_dir (GHashTable *savers_hash,
			      GList *l, const gchar *dirname)
{
	DIR *dir;
	struct dirent *dent;
	gchar *filename;
	Screensaver *saver;

	g_return_val_if_fail (savers_hash != NULL, NULL);
	g_return_val_if_fail (dirname != NULL, NULL);

	dir = opendir (dirname);
	if (!dir)
		return l;

	while ((dent = readdir (dir)))
	{
		if (dent->d_name[0] == '.')
			continue;

		filename = g_concat_dir_and_file (dirname, dent->d_name);
		saver = screensaver_new_from_file (filename);
		if (saver)
		{
			l = g_list_prepend (l, saver);
			g_hash_table_insert (savers_hash,
					     saver->name, saver);
			saver->link = l;
		}
		g_free (filename);
	}

	return l;
}

static gint
screensaver_cmp_func (gconstpointer a, gconstpointer b)
{
	const Screensaver *s1 = a;
	const Screensaver *s2 = b;

	return strcmp (s1->name, s2->name);
}

static GList*
screensaver_list_load (GHashTable *savers_hash)
{
	GList *l = NULL;
	
	gchar *userdir;

	l = screensaver_list_prepend_dir (savers_hash,
					  l, GNOMECC_DATA_DIR "/screensavers");
       	
	userdir = g_concat_dir_and_file (g_get_home_dir (), ".screensavers");
	l = screensaver_list_prepend_dir (savers_hash,
					  l, userdir);
	g_free (userdir);

	/* FIXME: Does not work with utf8 */
	l = g_list_sort (l, screensaver_cmp_func);
	return l;
}

Preferences *
preferences_new (void) 
{
	Preferences *prefs;

	prefs = g_new0 (Preferences, 1);

	prefs->savers_hash = g_hash_table_new (g_str_hash, g_str_equal);

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

static Screensaver*
screensaver_clone (Screensaver *oldsaver)
{
	Screensaver *saver;
	GList *l;
	g_return_val_if_fail (oldsaver != NULL, NULL);

	saver = screensaver_new ();
	saver->id = oldsaver->id;
	saver->name = g_strdup (oldsaver->name);
	saver->filename = g_strdup (oldsaver->filename);
	saver->label = g_strdup (oldsaver->label);
	saver->description = g_strdup (oldsaver->description);
	if (oldsaver->command_line)
		saver->command_line = g_strdup (oldsaver->command_line);
	if (oldsaver->compat_command_line)
		saver->compat_command_line = g_strdup (oldsaver->compat_command_line);
	if (oldsaver->visual)
		saver->visual = g_strdup (oldsaver->name);
	saver->enabled = oldsaver->enabled;
	if (oldsaver->fakepreview)
		saver->fakepreview = g_strdup (oldsaver->fakepreview);

	for (l = oldsaver->fakes; l != NULL; l = l->next)
	{
		saver->fakes = g_list_prepend (saver->fakes, g_strdup (l->data));
	}

	saver->fakes = g_list_reverse (saver->fakes);

	return saver;
}

static GList*
copy_screensavers (GList *screensavers, GHashTable *savers_hash)
{
	GList *ret = NULL, *l;
	Screensaver *saver;

	g_return_val_if_fail (savers_hash != NULL, NULL);

	for (l = screensavers; l != NULL; l = l->next)
	{
		saver = screensaver_clone (l->data);
		ret = g_list_prepend (ret, saver);
		g_hash_table_insert (savers_hash, saver->name, saver);
		saver->link = ret;
	}

	ret = g_list_reverse (ret);
	return ret;
}

Preferences *
preferences_clone (Preferences *prefs) 
{
	Preferences *new_prefs;

	new_prefs = g_new0 (Preferences, 1);
	
	new_prefs->savers_hash = g_hash_table_new (g_str_hash, g_str_equal);

	new_prefs->config_db = g_tree_new ((GCompareFunc) strcmp);

	new_prefs->selection_mode = prefs->selection_mode;
	new_prefs->power_management = prefs->power_management;
	new_prefs->standby_time = prefs->standby_time;
	new_prefs->suspend_time = prefs->suspend_time;
	new_prefs->power_down_time = prefs->power_down_time;

	g_tree_traverse (prefs->config_db, (GTraverseFunc) clone_cb,
			 G_IN_ORDER, new_prefs);

	new_prefs->screensavers = copy_screensavers (prefs->screensavers, 
						     new_prefs->savers_hash);
			
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
	g_hash_table_destroy (prefs->savers_hash);
	g_free (prefs);
}

static void
clean_saver_list (Preferences *prefs)
{
	GList *l, *next;
	Screensaver *saver;
	
	l = prefs->screensavers;
	while (l)
	{
		saver = l->data;
	
		if ((saver->command_line
		 && !rc_command_exists (saver->command_line))
		 || (saver->compat_command_line
		 && !rc_command_exists (saver->compat_command_line))
		 || !(saver->command_line || saver->compat_command_line))
		{
			prefs->invalidsavers = g_list_append (prefs->invalidsavers, l->data); 
			next = l->next;
			prefs->screensavers = g_list_remove_link (prefs->screensavers, l);
			l = next;
			continue;
		}
		l = l->next;
	}
}

void
preferences_load (Preferences *prefs) 
{
	GConfClient *client;
	
	g_return_if_fail (prefs != NULL);

	if (!preferences_load_from_file (prefs))
		preferences_load_from_xrdb (prefs);

	g_assert (prefs->screensavers == NULL);
	prefs->screensavers = screensaver_list_load (prefs->savers_hash);

	read_prefs_from_db (prefs);
	clean_saver_list (prefs);

	client = gconf_client_get_default ();

	prefs->selection_mode =
		gconf_client_get_int (client,
				      "/apps/screensaver/selection_mode",
				      NULL);
	prefs->power_management = 
		gconf_client_get_bool (client,
				       "/apps/screensaver/use_dpms",
				       NULL);
	prefs->standby_time = 
		gconf_client_get_int (client,
				      "/apps/screensaver/standby_time",
				      NULL);
	prefs->suspend_time = 
		gconf_client_get_int (client,
				      "/apps/screensaver/suspend_time",
				      NULL);
	prefs->power_down_time = 
		gconf_client_get_int (client,
				      "/apps/screensaver/shutdown_time",
				      NULL);

	g_object_unref (G_OBJECT (client));
}

void 
preferences_save (Preferences *prefs) 
{
	GConfClient *client;

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (prefs->config_db != NULL);

	store_prefs_in_db (prefs);

	preferences_save_to_file (prefs);
	
	client = gconf_client_get_default ();

	gconf_client_set_int (client,
			      "/apps/screensaver/selection_mode",
			      prefs->selection_mode, NULL);
	gconf_client_set_bool (client,
			       "/apps/screensaver/use_dpms",
			       prefs->power_management, NULL);
	gconf_client_set_int (client,
			      "/apps/screensaver/standby_time",
			      prefs->standby_time, NULL);
	gconf_client_set_int (client,
			      "/apps/screensaver/suspend_time",
			      prefs->suspend_time, NULL);
	gconf_client_set_int (client,
			      "/apps/screensaver/shutdown_time",
			      prefs->power_down_time, NULL);
	
	g_object_unref (G_OBJECT (client));
}

static GList *
xml_get_programs_list (xmlNodePtr programs_node) 
{
	GList *list_head = NULL, *list_tail = NULL;
	xmlNodePtr node;
	Screensaver *saver;
	gint id = 0;

	for (node = programs_node->children; node; node = node->next) {
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

	for (node = root_node->children; node; node = node->next) {
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
#if 0
		else if (!strcmp (node->name, "programs"))
			prefs->screensavers = xml_get_programs_list (node);
#endif
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
	GList *l;
	
	if (!saver) return;

	if (saver->visual) g_free (saver->visual);
	if (saver->name) g_free (saver->name);
	if (saver->filename) g_free (saver->filename);
	if (saver->command_line) g_free (saver->command_line);
	if (saver->compat_command_line) g_free (saver->compat_command_line);
	if (saver->label) g_free (saver->label);
	if (saver->description) g_free (saver->description);
	if (saver->fakepreview) g_free (saver->fakepreview);

	for (l = saver->fakes; l != NULL; l = l->next)
	{
		g_free (l->data);
	}

	if (saver->fakes)
		g_list_free (saver->fakes);
	
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
#if 0
static void
parse_select_default (GString *s, xmlNodePtr node)
{
	for (node = node->childs; node != NULL; node = node->next)
	{
		if (strcmp (node->name, "option"))
			continue;
		if (xmlGetProp (node, "test"))
			continue;
		g_string_append (s, xmlGetProp (node, ""
	}
}
#endif

static void
parse_arg_default (GString *s, xmlNodePtr node)
{
	gchar *arg;
	gchar *val;
	gchar **arr;
	
	arg = g_strdup (xmlGetProp (node, "arg"));
	val = g_strdup (xmlGetProp (node, "default"));
	if (!val)
		return;
	arr = g_strsplit (arg, "%", -1);
	if (!arr)
		return;

	g_string_append_c (s, ' ');
	g_string_append (s, arr[0]);
	g_string_append (s, val);
	if (arr[1] && arr[2])
		g_string_append (s, arr[2]);
}

Screensaver *
screensaver_read_xml (xmlNodePtr saver_node) 
{
	Screensaver *saver;
	xmlNodePtr node;
	GString *args;
	gboolean have_args = FALSE;

	if (strcmp (saver_node->name, "screensaver"))
		return NULL;

	saver = screensaver_new ();
	saver->enabled = FALSE;

	saver->name = g_strdup (xmlGetProp (saver_node, "name"));
	saver->label = g_strdup (xmlGetProp (saver_node, "_label"));

	args = g_string_new (saver->name);

	for (node = saver_node->children; node; node = node->next) {
		if (!strcmp (node->name, "command-line"))
			saver->command_line =
				g_strdup (xmlNodeGetContent (node));
		else if (!strcmp (node->name, "visual"))
			saver->visual = g_strdup (xmlNodeGetContent (node));
		else if (!strcmp (node->name, "enabled"))
			saver->enabled = xml_read_bool (node);
		else if (!strcmp (node->name, "_description"))
			saver->description = g_strdup (xmlNodeGetContent (node));
		else if (!strcmp (node->name, "fullcommand"))
			saver->compat_command_line = g_strconcat (saver->name, " ",  xmlGetProp (node, "arg"), NULL);
		else if (!strcmp (node->name, "number"))
		{
			parse_arg_default (args, node);
			have_args = TRUE;
		}
		else if (!strcmp (node->name, "command"))
		{
			g_string_append_c (args, ' ');
			g_string_append (args, xmlGetProp (node, "arg"));
			have_args = TRUE;
		}
		else if (!strcmp (node->name, "fakepreview"))
		{
			saver->fakepreview = gnome_program_locate_file
				(gnome_program_get (), GNOME_FILE_DOMAIN_APP_PIXMAP,
				 xmlNodeGetContent (node), TRUE, NULL);
		}
		else if (!strcmp (node->name, "fake"))
		{
			saver->fakes = g_list_append (saver->fakes, g_strdup (xmlGetProp (node, "name")));
		}
	}

	if (have_args)
	{
		if (saver->compat_command_line)
		{
			g_warning ("Huh? Argument metadata and a fullcommand?");
		}
		else
			saver->command_line = g_strdup (args->str);
	}

	g_string_free (args, TRUE);
	
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

Screensaver *
screensaver_new_from_file (const gchar *filename)
{
	Screensaver *saver;
	xmlDocPtr doc;
	xmlNodePtr node;
	
	g_return_val_if_fail (filename != NULL, NULL);

	doc = xmlParseFile (filename);
	if (!doc)
		return NULL;

	node = doc->children;
	if (!node)
	{
		xmlFreeDoc (doc);
		return NULL;
	}
	
	saver = screensaver_read_xml (node);
	if (saver)
		saver->filename = g_strdup (filename);
	xmlFreeDoc (doc);

	return saver;
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
