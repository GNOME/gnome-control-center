/* -*- mode: c; style: linux -*- */

/* preferences.c
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen <hovinen@helixcode.com>,
 *            Elliot Lee <sopwith@redhat.com>
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

#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>

#include <gnome.h>
#include <esd.h>

#include "preferences.h"

static GtkObjectClass *parent_class;

typedef struct _triple_t { gpointer a; gpointer b; gpointer c; } triple_t;

static void preferences_init             (Preferences *prefs);
static void preferences_class_init       (PreferencesClass *class);

static gint       xml_read_int           (xmlNodePtr node);
static xmlNodePtr xml_write_int          (gchar *name, 
					  gint number);
static gboolean   xml_read_bool          (xmlNodePtr node);
static xmlNodePtr xml_write_bool         (gchar *name,
					  gboolean value);

static gint apply_timeout_cb             (Preferences *prefs);

static void read_sound_events_from_xml   (xmlNodePtr events_node,
					  GTree *tree);
static xmlNodePtr write_sound_events_to_xml (GTree *events);

static void read_path                    (Preferences *prefs, 
					  const char *path);

static void reload_esd_samples           (const char *config_path);
static void reload_all_esd_samples       (void);

static Category *category_new            (void);
static void category_destroy             (Category *category);
static Category *category_clone          (Category *category);

static xmlNodePtr category_write_xml     (Category *category);
static Category *category_read_xml       (xmlNodePtr event_node);

static SoundEvent *sound_event_new       (void);
static void sound_event_destroy          (SoundEvent *event);
static SoundEvent *sound_event_clone     (SoundEvent *event);

static void sound_event_set_file         (SoundEvent *event, gchar *file);

static xmlNodePtr sound_event_write_xml  (SoundEvent *event);
static SoundEvent *sound_event_read_xml  (xmlNodePtr event_node);

static void start_esd                    (void);

GType
preferences_get_type (void)
{
	static GType preferences_type = 0;

	if (!preferences_type) {
		GtkTypeInfo preferences_info = {
			"Preferences",
			sizeof (Preferences),
			sizeof (PreferencesClass),
			(GtkClassInitFunc) preferences_class_init,
			(GtkObjectInitFunc) preferences_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		preferences_type = 
			gtk_type_unique (gtk_object_get_type (), 
					 &preferences_info);
	}

	return preferences_type;
}

static void
preferences_init (Preferences *prefs)
{
	gchar *ctmp;

	prefs->frozen              = FALSE;
	prefs->categories          = g_tree_new ((GCompareFunc) strcmp);
	prefs->cat_byfile          = g_tree_new ((GCompareFunc) strcmp);

	/* Load default values */
	prefs->enable_esd          = FALSE;
	prefs->enable_sound_events = FALSE;

	ctmp = gnome_config_file ("/sound/events");
	if (ctmp != NULL) {
		read_path (prefs, ctmp);
		g_free (ctmp);
	}
}

static void
preferences_class_init (PreferencesClass *class) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;
	object_class->destroy = preferences_destroy;

	parent_class = 
		GTK_OBJECT_CLASS (gtk_type_class (gtk_object_get_type ()));
}

GtkObject *
preferences_new (void) 
{
	GtkObject *object;

	object = gtk_type_new (preferences_get_type ());

	return object;
}

static gint
tree_clone_cb (gchar *cat_name, Category *category, GTree *new_tree) 
{
	Category *new_cat;

	new_cat = category_clone (category);
	g_tree_insert (new_tree, new_cat->file, new_cat);

	return 0;
}

GtkObject *
preferences_clone (Preferences *prefs)
{
	GtkObject *object;
	Preferences *new_prefs;

	g_return_val_if_fail (prefs != NULL, NULL);
	g_return_val_if_fail (IS_PREFERENCES (prefs), NULL);

	object = preferences_new ();

	new_prefs = PREFERENCES (object);
	new_prefs->enable_esd = prefs->enable_esd;
	new_prefs->enable_sound_events = prefs->enable_sound_events;

	g_tree_traverse (prefs->categories, (GTraverseFunc) tree_clone_cb,
			 G_IN_ORDER, new_prefs->categories);

	return object;
}

static gint
tree_destroy_cb (gchar *cat_name, Category *category) 
{
	category_destroy (category);
	return 0;
}

void
preferences_destroy (GtkObject *object) 
{
	Preferences *prefs;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_PREFERENCES (object));

	prefs = PREFERENCES (object);

	if (prefs->categories) {
		g_tree_traverse (prefs->categories, 
				 (GTraverseFunc) tree_destroy_cb, 
				 G_IN_ORDER, NULL);
		g_tree_destroy (prefs->categories);
	}

	if (prefs->cat_byfile)
		g_tree_destroy (prefs->cat_byfile);

	parent_class->destroy (object);
}

void
preferences_load (Preferences *prefs) 
{
	gchar *ctmp;

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->enable_esd =
		gnome_config_get_bool
		("/sound/system/settings/start_esd=false");
	prefs->enable_sound_events =
		gnome_config_get_bool
		("/sound/system/settings/event_sounds=false");

	ctmp = gnome_config_file ("/sound/events");
	if (ctmp != NULL) {
		read_path (prefs, ctmp);
		g_free (ctmp);
	}

	ctmp = gnome_util_home_file ("sound/events");
	if (ctmp != NULL) {
		read_path (prefs, ctmp);
		g_free (ctmp);
	}
}

static gint
event_save_cb (gchar *event_name, SoundEvent *event, Category *category) 
{
	gchar *str;

	str = g_strconcat ("/sound/events/", category->file, "/",
			   event->name, "/file", NULL);
	gnome_config_set_string (str, event->file);
	g_free (str);

	return 0;
}

static gint
tree_save_cb (gchar *cat_name, Category *category) 
{
	g_tree_traverse (category->events, (GTraverseFunc) event_save_cb,
			 G_IN_ORDER, category);
	return 0;
}

void 
preferences_save (Preferences *prefs) 
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	gnome_config_set_bool ("/sound/system/settings/start_esd",
			       prefs->enable_esd);
	gnome_config_set_bool ("/sound/system/settings/event_sounds",
			       prefs->enable_sound_events && 
			       prefs->enable_esd);

	g_tree_traverse (prefs->categories, (GTraverseFunc) tree_save_cb,
			 G_IN_ORDER, NULL);

	gnome_config_sync ();
}

void
preferences_changed (Preferences *prefs) 
{
	if (prefs->frozen) return;

	if (prefs->timeout_id)
		gtk_timeout_remove (prefs->timeout_id);

/* 	prefs->timeout_id = gtk_timeout_add (1000,  */
/* 					     (GtkFunction) apply_timeout_cb, */
/* 					     prefs); */
}

void
preferences_apply_now (Preferences *prefs)
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	if (prefs->timeout_id)
		gtk_timeout_remove (prefs->timeout_id);

	prefs->timeout_id = 0;

	if (prefs->enable_esd && gnome_sound_connection < 0)
		start_esd ();

	if (prefs->enable_esd && prefs->enable_sound_events)
		reload_all_esd_samples ();
}

void
preferences_freeze (Preferences *prefs) 
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	prefs->frozen++;
}

void
preferences_thaw (Preferences *prefs) 
{
	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));

	if (prefs->frozen > 0) prefs->frozen--;
}

Preferences *
preferences_read_xml (xmlDocPtr xml_doc) 
{
	Preferences *prefs;
	xmlNodePtr root_node, node;

	prefs = PREFERENCES (preferences_new ());

	root_node = xmlDocGetRootElement (xml_doc);

	if (strcmp (root_node->name, "sound-properties"))
		return NULL;

	for (node = root_node->childs; node; node = node->next) {
		if (!strcmp (node->name, "enable-esd"))
			prefs->enable_esd = xml_read_bool (node);
		else if (!strcmp (node->name, "enable-sound-events"))
			prefs->enable_sound_events = xml_read_bool (node);
		else if (!strcmp (node->name, "categories"))
			read_sound_events_from_xml (node, prefs->categories);
	}

	return prefs;
}

xmlDocPtr 
preferences_write_xml (Preferences *prefs) 
{
	xmlDocPtr doc;
	xmlNodePtr node;

	doc = xmlNewDoc ("1.0");

	node = xmlNewDocNode (doc, NULL, "sound-properties", NULL);

	xmlAddChild (node, xml_write_bool ("enable-esd", prefs->enable_esd));
	xmlAddChild (node, xml_write_bool ("enable-sound-events",
					   prefs->enable_sound_events));
	xmlAddChild (node, write_sound_events_to_xml (prefs->categories));

	xmlDocSetRootElement (doc, node);

	return doc;
}

static gint
tree_cb (gchar *key, Category *category, triple_t *triple) 
{
	return ((CategoryCallback) triple->b) (PREFERENCES (triple->a), 
					       category->description,
					       triple->c);
}

void
preferences_foreach_category (Preferences *prefs, CategoryCallback cb,
			      gpointer data)
{
	triple_t p;

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (cb != NULL);

	p.a = prefs; p.b = cb; p.c = data;
	g_tree_traverse (prefs->categories, (GTraverseFunc) tree_cb, 
			 G_IN_ORDER, &p);
}

static int
event_tree_cb (gchar *key, SoundEvent *event, triple_t *triple) 
{
	return ((EventCallback) triple->b) (PREFERENCES (triple->a), event, 
					    triple->c);
}

void
preferences_foreach_event (Preferences *prefs, gchar *cat_name,
			   EventCallback cb, gpointer data)
{
	Category *category;
	triple_t p;

	g_return_if_fail (prefs != NULL);
	g_return_if_fail (IS_PREFERENCES (prefs));
	g_return_if_fail (cat_name != NULL);
	g_return_if_fail (cb != NULL);

	category = g_tree_lookup (prefs->categories, cat_name);
	if (category == NULL) return;

	p.a = prefs; p.b = cb; p.c = data;
	g_tree_traverse (category->events, (GTraverseFunc) event_tree_cb, 
			 G_IN_ORDER, &p);
}

static void
read_sound_events_from_xml (xmlNodePtr events_node, GTree *tree)
{
	xmlNodePtr node;
	Category *category;

	for (node = events_node->childs; node; node = node->next) {
		if (!strcmp (node->name, "category")) {
			category = category_read_xml (node);
			g_tree_insert (tree, category->description, category);
		}
	}
}

static gint
tree_write_cb (gchar *cat_name, Category *category, xmlNodePtr node) 
{
	xmlAddChild (node, category_write_xml (category));
	return 0;
}

static xmlNodePtr
write_sound_events_to_xml (GTree *events)
{
	xmlNodePtr node;

	g_return_val_if_fail (events != NULL, NULL);

	node = xmlNewNode (NULL, "categories");
	g_tree_traverse (events, (GTraverseFunc) tree_write_cb, 
			 G_IN_ORDER, node);
	return node;
}

static void
read_path (Preferences *prefs, const char *path)
{
	DIR *dirh;
	Category *new_cat;
	SoundEvent *new_event;
	char *sample_name, *ctmp, *prefix;
	gpointer event_iter;
	struct dirent *dent;
	gboolean is_new, event_is_new;

	dirh = opendir (path);

	if (dirh == NULL)
		return;

	while ((dent = readdir (dirh))) {
		if (!strcmp (dent->d_name, ".")
		    || !strcmp (dent->d_name, ".."))
			continue;

		prefix = g_strdup_printf ("=%s/%s=", path, dent->d_name);
		gnome_config_push_prefix (prefix);

		new_cat = g_tree_lookup (prefs->cat_byfile, dent->d_name);

		if (new_cat != NULL) {
			is_new = FALSE;
		} else {
			is_new = TRUE;
			new_cat = category_new ();
			new_cat->file = g_strdup (dent->d_name);
			new_cat->description = 
				gnome_config_get_translated_string
				("__section_info__/description");
		}

		event_iter = gnome_config_init_iterator_sections (prefix);
		g_free (prefix);

		while ((event_iter = gnome_config_iterator_next
			(event_iter, &sample_name, NULL))) 
		{
			if (!strcmp (sample_name, "__section_info__")) {
				g_free (sample_name);
				continue;
			}

			new_event = g_tree_lookup (new_cat->events,
						   sample_name);

			if (new_event != NULL) {
				event_is_new = FALSE;
			} else {
				event_is_new = TRUE;
				new_event = sound_event_new ();
				new_event->name = sample_name;
				new_event->category = new_cat;
				ctmp = g_strdup_printf
					("%s/description", sample_name);
				new_event->description =
					gnome_config_get_translated_string 
					(ctmp);
				g_free (ctmp);
			}

			prefix = g_strdup_printf ("%s/file", sample_name);
			sound_event_set_file (new_event,
					      gnome_config_get_string
					      (prefix));
			g_free (prefix);

			if (event_is_new) {
				g_tree_insert (new_cat->events, sample_name,
					       new_event);
			} else {
				g_free(sample_name);
			}
		}

		gnome_config_pop_prefix();

		if (is_new && new_cat->description != NULL) {
			g_tree_insert (prefs->categories, new_cat->description,
				       new_cat);
			g_tree_insert (prefs->cat_byfile, new_cat->file,
				       new_cat);
		}
		else if (is_new) {
			category_destroy (new_cat);
		}
	}

	closedir(dirh);
}

static void
reload_esd_samples(const char *config_path)
{
	DIR *dirh;
	char *category_name, *sample_name, *sample_file, *ctmp;
	gpointer event_iter;
	struct dirent *dent;
	GString *tmpstr;

	dirh = opendir(config_path);
	if(!dirh)
		return;

	tmpstr = g_string_new(NULL);

	while((dent = readdir(dirh))) {
		/* ignore no-good dir entries.
		   We ignore "gnome" because the system sounds are listed in there.
		*/

		if (!strcmp(dent->d_name, ".")
		    || !strcmp(dent->d_name, ".."))
			continue;

		g_string_sprintf(tmpstr, "=%s/%s=", config_path, dent->d_name);

		gnome_config_push_prefix(tmpstr->str);

		category_name = dent->d_name;
		ctmp = strstr(category_name, ".soundlist");
		if(ctmp) *ctmp = '\0';

		event_iter = gnome_config_init_iterator_sections(tmpstr->str);
		while((event_iter = gnome_config_iterator_next(event_iter,
							       &sample_name, NULL))) {
			if(!strcmp(sample_name, "__section_info__")) {
				g_free(sample_name);
				continue;
			}

			g_string_sprintf(tmpstr, "%s/file", sample_name);
			sample_file = gnome_config_get_string(tmpstr->str);

			if(!sample_file || !*sample_file) {
				g_free(sample_file);
				g_free(sample_name);
				continue;
			}

			if(*sample_file != '/') {
				char *tmp = gnome_sound_file(sample_file);
				g_free(sample_file);
				sample_file = tmp;
			}

			if(sample_file) {
				int sid;

				g_string_sprintf(tmpstr, "%s/%s", category_name, sample_name);

				/* We need to free up the old sample, because
				   esd allows multiple samples with the same name,
				   putting memory to waste. */
				sid = esd_sample_getid(gnome_sound_connection, tmpstr->str);
				if(sid >= 0)
					esd_sample_free(gnome_sound_connection, sid);

				sid = gnome_sound_sample_load(tmpstr->str, sample_file);

				if(sid < 0)
					g_warning("Couldn't load sound file %s as sample %s",
						  sample_file, tmpstr->str);
			}
            
			g_free(sample_name);
			g_free(sample_file);
		}

		gnome_config_pop_prefix();
	}
	closedir(dirh);

	g_string_free(tmpstr, TRUE);
}

static void
reload_all_esd_samples(void) 
{
	char *val;
	val = gnome_config_file("/sound/events");
	if(val) {
		reload_esd_samples(val);
		g_free(val);
	}
	val = gnome_util_home_file("/sound/events");
	if(val) {
		reload_esd_samples(val);
		g_free(val);
	}
}

static Category *
category_new (void)
{
	Category *category;

	category = g_new0 (Category, 1);
	category->events = g_tree_new ((GCompareFunc) strcmp);

	return category;
}

static int
events_tree_destroy_cb (gchar *event_name, SoundEvent *event) 
{
	sound_event_destroy (event);
	return 0;
}

static void
category_destroy (Category *category)
{
	g_tree_traverse (category->events,
			 (GTraverseFunc) events_tree_destroy_cb, G_IN_ORDER,
			 NULL);
	g_tree_destroy (category->events);

	if (category->description)
		g_free (category->description);
	g_free (category->file);
	g_free (category);
}

static int
event_tree_clone_cb (gchar *event_name, SoundEvent *event, GTree *new_tree) 
{
	g_tree_insert (new_tree, event_name, sound_event_clone (event));
	return 0;
}

static Category *
category_clone (Category *category) 
{
	Category *new_cat;

	g_return_val_if_fail (category != NULL, NULL);

	new_cat = category_new ();
	if (category->file != NULL)
		new_cat->file = g_strdup (category->file);
	if (category->description != NULL)
		new_cat->description = g_strdup (category->description);

	g_tree_traverse (category->events,
			 (GTraverseFunc) event_tree_clone_cb, G_IN_ORDER,
			 new_cat->events);

	return new_cat;
}

static Category *
category_read_xml (xmlNodePtr cat_node)
{
	Category *category;
	xmlNodePtr node;
	GList *list_head = NULL, *list_tail = NULL;
	SoundEvent *event;

	g_return_val_if_fail (cat_node != NULL, NULL);
	g_return_val_if_fail (strcmp (cat_node->name, "category"), NULL);

	category = category_new ();

	category->file = xmlGetProp (cat_node, "file");

	for (node = cat_node->childs; node; node = node->next) {
		if (!strcmp (node->name, "description"))
			category->description = xmlNodeGetContent (node);
		else if (!strcmp (node->name, "event")) {
			event = sound_event_read_xml (node);
			list_tail = g_list_append (list_tail, event);
			if (list_head == NULL)
				list_head = list_tail;
			else
				list_tail = list_tail->next;
		}
	}

	return category;
}

static int
event_tree_write_xml_cb (gchar *event_name, SoundEvent *event, 
			 xmlNodePtr node) 
{
	xmlAddChild (node, sound_event_write_xml (event));
	return 0;
}


static xmlNodePtr
category_write_xml (Category *category)
{
	xmlNodePtr node;

	g_return_val_if_fail (category != NULL, NULL);

	node = xmlNewNode (NULL, "category");
	xmlNewProp (node, "file", category->file);
	xmlNewChild (node, NULL, "description", category->description);

	g_tree_traverse (category->events,
			 (GTraverseFunc) event_tree_write_xml_cb, G_IN_ORDER,
			 node);

	return node;
}

static SoundEvent *
sound_event_new (void)
{
	return g_new0 (SoundEvent, 1);
}

static SoundEvent *
sound_event_clone (SoundEvent *event) 
{
	SoundEvent *new_event;

	g_return_val_if_fail (event != NULL, NULL);
	g_return_val_if_fail (event->category != NULL, NULL);
	g_return_val_if_fail (event->name != NULL, NULL);

	new_event = sound_event_new ();
	new_event->category = event->category;
	new_event->name = g_strdup (event->name);
	if (event->file != NULL)
		new_event->file = g_strdup (event->file);

	return new_event;
}

static void
sound_event_destroy (SoundEvent *event)
{
	g_return_if_fail (event != NULL);

	if (event->name) g_free (event->name);
	if (event->file) g_free (event->file);
	g_free (event);
}

static void
sound_event_set_file (SoundEvent *event, gchar *file)
{
	g_return_if_fail (event != NULL);

	if (event->file)
		g_free (event->file);
	event->file = file;
}

static xmlNodePtr
sound_event_write_xml  (SoundEvent *event)
{
	xmlNodePtr node;

	g_return_val_if_fail (event != NULL, NULL);
	g_return_val_if_fail (event->category != NULL, NULL);
	g_return_val_if_fail (event->name != NULL, NULL);

	node = xmlNewNode (NULL, "event");
	xmlNewProp (node, "name", event->name);

	if (event->file != NULL)
		xmlNewChild (node, NULL, "file", event->file);

	return node;
}

static SoundEvent *
sound_event_read_xml  (xmlNodePtr event_node)
{
	SoundEvent *event;
	xmlNodePtr node;

	if (strcmp (event_node->name, "event"))
		return NULL;

	event = sound_event_new ();
	event->name = g_strdup (xmlGetProp (event_node, "name"));

	for (node = event_node->childs; node; node = node->next) {
		if (!strcmp (node->name, "file"))
			event->file = g_strdup (xmlNodeGetContent (node));
	}

	return event;
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

static gint 
apply_timeout_cb (Preferences *prefs) 
{
	preferences_apply_now (prefs);

	return TRUE;
}

static void
start_esd (void) 
{
#ifdef HAVE_ESD
	int esdpid;
	static const char *esd_cmdline[] = {"esd", "-nobeeps", NULL};
	char *tmpargv[3];
	char argbuf[32];
	time_t starttime;
	GnomeClient *client = gnome_master_client ();

	esdpid = gnome_execute_async(NULL, 2, (char **)esd_cmdline);
	g_snprintf(argbuf, sizeof(argbuf), "%d", esdpid);
	tmpargv[0] = "kill"; tmpargv[1] = argbuf; tmpargv[2] = NULL;
	gnome_client_set_shutdown_command(client, 2, tmpargv);
	starttime = time(NULL);
	gnome_sound_init(NULL);
	while(gnome_sound_connection < 0
	      && ((time(NULL) - starttime) < 4)) 
	{
#ifdef HAVE_USLEEP
		usleep(1000);
#endif
		gnome_sound_init(NULL);
	}
#endif
}
