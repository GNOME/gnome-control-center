/**
 * bonobo-config-archiver.c: XML configuration database with an interface to
 * the XML archiver
 *
 * Author:
 *   Dietmar Maurer (dietmar@ximian.com)
 *   Bradford Hovinen <hovinen@ximian.com>
 *
 * Copyright 2000, 2001 Ximian, Inc.
 */

#include <config.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <bonobo.h>
#include <bonobo-conf/bonobo-config-utils.h>
#include <gnome-xml/xmlmemory.h>
#include <gtk/gtkmain.h>

#include "util.h"
#include "bonobo-config-archiver.h"

static GtkObjectClass *parent_class = NULL;

#define CLASS(o) BONOBO_CONFIG_ARCHIVER_CLASS (GTK_OBJECT(o)->klass)

#define PARENT_TYPE BONOBO_CONFIG_DATABASE_TYPE
#define FLUSH_INTERVAL 30 /* 30 seconds */

/* Small Bonobo bug here... */

#undef BONOBO_RET_EX
#define BONOBO_RET_EX(ev)		\
	G_STMT_START{			\
		if (BONOBO_EX (ev))	\
			return;		\
	}G_STMT_END

extern int daytime;

static DirEntry *
dir_lookup_entry (DirData  *dir,
		  char     *name,
		  gboolean  create)
{
	GSList *l;
	DirEntry *de;
	
	l = dir->entries;

	while (l) {
		de = (DirEntry *)l->data;

		if (!strcmp (de->name, name))
			return de;
		
		l = l->next;
	}

	if (create) {

		de = g_new0 (DirEntry, 1);
		
		de->dir = dir;

		de->name = g_strdup (name);

		dir->entries = g_slist_prepend (dir->entries, de);

		return de;
	}

	return NULL;
}

static DirData *
dir_lookup_subdir (DirData  *dir,
		   char     *name,
		   gboolean  create)
{
	GSList *l;
	DirData *dd;
	
	l = dir->subdirs;

	while (l) {
		dd = (DirData *)l->data;

		if (!strcmp (dd->name, name))
			return dd;
		
		l = l->next;
	}

	if (create) {

		dd = g_new0 (DirData, 1);

		dd->dir = dir;

		dd->name = g_strdup (name);

		dir->subdirs = g_slist_prepend (dir->subdirs, dd);

		return dd;
	}

	return NULL;
}

static DirData *
lookup_dir (DirData    *dir,
	    const char *path,
	    gboolean    create)
{
	const char *s, *e;
	char *name;
	DirData  *dd = dir;
	
	s = path;
	while (*s == '/') s++;
	
	if (*s == '\0')
		return dir;

	if ((e = strchr (s, '/')))
		name = g_strndup (s, e - s);
	else
		name = g_strdup (s);
	
	if ((dd = dir_lookup_subdir (dd, name, create))) {
		if (e)
			dd = lookup_dir (dd, e, create);

		g_free (name);
		return dd;
		
	} else {
		g_free (name);
	}

	return NULL;

}

static DirEntry *
lookup_dir_entry (BonoboConfigArchiver *archiver_db,
		  const char        *key, 
		  gboolean           create)
{
	char *dir_name;
	char *leaf_name;
	DirEntry *de;
	DirData  *dd;

	if ((dir_name = bonobo_config_dir_name (key))) {
		dd = lookup_dir (archiver_db->dir, dir_name, create);
		
		if (dd && !dd->node) {
			dd->node = xmlNewChild (archiver_db->doc->root, NULL, 
						"section", NULL);
		
			xmlSetProp (dd->node, "path", dir_name);
		}	

		g_free (dir_name);

	} else {
		dd = archiver_db->dir;

		if (!dd->node) 
			dd->node = xmlNewChild (archiver_db->doc->root, NULL, 
						"section", NULL);
	}

	if (!dd)
		return NULL;

	if (!(leaf_name = bonobo_config_leaf_name (key)))
		return NULL;

	de = dir_lookup_entry (dd, leaf_name, create);

	g_free (leaf_name);

	return de;
}

static CORBA_any *
real_get_value (BonoboConfigDatabase *db,
		const CORBA_char     *key, 
		const CORBA_char     *locale, 
		CORBA_Environment    *ev)
{
	BonoboConfigArchiver *archiver_db = BONOBO_CONFIG_ARCHIVER (db);
	DirEntry          *de;
	CORBA_any         *value = NULL;

	de = lookup_dir_entry (archiver_db, key, FALSE);
	if (!de) {
		bonobo_exception_set (ev, ex_Bonobo_ConfigDatabase_NotFound);
		return NULL;
	}

	if (de->value)
		return bonobo_arg_copy (de->value);
	
	/* localized value */
	if (de->node && de->node->childs && de->node->childs->next)	
		value = bonobo_config_xml_decode_any ((BonoboUINode *)de->node,
						      locale, ev);

	if (!value)
		bonobo_exception_set (ev, ex_Bonobo_ConfigDatabase_NotFound);
	
	return value;
}

/* Produce a new, dynamically allocated string with the OAF IID of the listener
 * associated with the given backend id
 */

static gchar *
get_listener_oafiid (const gchar *backend_id) 
{
	gchar *oafiid, *tmp, *tmp1;

	tmp = g_strdup (backend_id);
	if ((tmp1 = strstr (tmp, "-properties")) != NULL) *tmp1 = '\0';
	oafiid = g_strconcat ("OAFIID:Bonobo_Listener_Config_", tmp, NULL);
	g_free (tmp);

	return oafiid;
}

static void
real_sync (BonoboConfigDatabase *db, 
	   CORBA_Environment    *ev)
{
	BonoboConfigArchiver *archiver_db = BONOBO_CONFIG_ARCHIVER (db);
	BonoboArg            *arg;

	gchar                *listener_oafiid;
	Bonobo_Listener       listener;

	if (!db->writeable)
		return;

	/* FIXME: This will not work correctly in the pathlogical case that two
	 * ConfigArchiver objects sync at almost exactly the same time.
	 */

	archiver_db->is_up_to_date = TRUE;
	location_client_store_xml (archiver_db->location, archiver_db->backend_id, 
				   archiver_db->doc, ConfigArchiver_STORE_MASK_PREVIOUS, ev);

	BONOBO_RET_EX (ev);

	/* Try to find a listener to apply the settings. If we can't, don't
	 * worry about it
	 */

	listener_oafiid = get_listener_oafiid (archiver_db->backend_id);
	listener = bonobo_get_object (listener_oafiid, "IDL:Bonobo/Listener:1.0", ev);
	g_free (listener_oafiid);

	if (BONOBO_EX (ev)) {
		listener = CORBA_OBJECT_NIL;
		CORBA_exception_init (ev);
	}

	arg = bonobo_arg_new (BONOBO_ARG_NULL);
	bonobo_event_source_notify_listeners (archiver_db->es, "Bonobo/ConfigDatabase:sync", arg, ev);
	bonobo_arg_release (arg);

	if (listener != CORBA_OBJECT_NIL)
		bonobo_object_release_unref (listener, NULL);
}

static void
notify_listeners (BonoboConfigArchiver *archiver_db, 
		  const char           *key, 
		  const CORBA_any      *value)
{
	CORBA_Environment ev;
	char *dir_name;
	char *leaf_name;
	char *ename;

	if (GTK_OBJECT_DESTROYED (archiver_db))
		return;

	if (!key)
		return;

	CORBA_exception_init (&ev);

	ename = g_strconcat ("Bonobo/Property:change:", key, NULL);

	bonobo_event_source_notify_listeners (archiver_db->es, ename, value, &ev);

	g_free (ename);
	
	if (!(dir_name = bonobo_config_dir_name (key)))
		dir_name = g_strdup ("");

	if (!(leaf_name = bonobo_config_leaf_name (key)))
		leaf_name = g_strdup ("");
	
	ename = g_strconcat ("Bonobo/ConfigDatabase:change", dir_name, ":", 
			     leaf_name, NULL);

	bonobo_event_source_notify_listeners (archiver_db->es, ename, value, &ev);
						   
	CORBA_exception_free (&ev);

	g_free (ename);
	g_free (dir_name);
	g_free (leaf_name);
}

static void
real_set_value (BonoboConfigDatabase *db,
		const CORBA_char     *key, 
		const CORBA_any      *value,
		CORBA_Environment    *ev)
{
	BonoboConfigArchiver *archiver_db = BONOBO_CONFIG_ARCHIVER (db);
	DirEntry *de;
	char *name;

	de = lookup_dir_entry (archiver_db, key, TRUE);

	if (de->value)
		CORBA_free (de->value);

	de->value = bonobo_arg_copy (value);

	if (de->node) {
		xmlUnlinkNode (de->node);
		xmlFreeNode (de->node);
	}
		
	name =  bonobo_config_leaf_name (key);

	de->node = (xmlNodePtr) bonobo_config_xml_encode_any (value, name, ev);
	
	g_free (name);
       
	bonobo_ui_node_add_child ((BonoboUINode *)de->dir->node, 
				  (BonoboUINode *)de->node);

	notify_listeners (archiver_db, key, value);
}

static Bonobo_KeyList *
real_list_dirs (BonoboConfigDatabase *db,
		const CORBA_char     *dir,
		CORBA_Environment    *ev)
{
	BonoboConfigArchiver *archiver_db = BONOBO_CONFIG_ARCHIVER (db);
	Bonobo_KeyList *key_list;
	DirData *dd, *sub;
	GSList *l;
	int len;
	
	key_list = Bonobo_KeyList__alloc ();
	key_list->_length = 0;

	if (!(dd = lookup_dir (archiver_db->dir, dir, FALSE)))
		return key_list;

	if (!(len = g_slist_length (dd->subdirs)))
		return key_list;

	key_list->_buffer = CORBA_sequence_CORBA_string_allocbuf (len);
	CORBA_sequence_set_release (key_list, TRUE); 
	
	for (l = dd->subdirs; l != NULL; l = l->next) {
		sub = (DirData *)l->data;
	       
		key_list->_buffer [key_list->_length] = 
			CORBA_string_dup (sub->name);
		key_list->_length++;
	}
	
	return key_list;
}

static Bonobo_KeyList *
real_list_keys (BonoboConfigDatabase *db,
		const CORBA_char     *dir,
		CORBA_Environment    *ev)
{
	BonoboConfigArchiver *archiver_db = BONOBO_CONFIG_ARCHIVER (db);
	Bonobo_KeyList *key_list;
	DirData *dd;
	DirEntry *de;
	GSList *l;
	int len;
	
	key_list = Bonobo_KeyList__alloc ();
	key_list->_length = 0;

	if (!(dd = lookup_dir (archiver_db->dir, dir, FALSE)))
		return key_list;

	if (!(len = g_slist_length (dd->entries)))
		return key_list;

	key_list->_buffer = CORBA_sequence_CORBA_string_allocbuf (len);
	CORBA_sequence_set_release (key_list, TRUE); 
	
	for (l = dd->entries; l != NULL; l = l->next) {
		de = (DirEntry *)l->data;

                key_list->_buffer [key_list->_length] = 
                        CORBA_string_dup (de->name);
                key_list->_length++;
	}
	
	return key_list;
}

static CORBA_boolean
real_dir_exists (BonoboConfigDatabase *db,
		 const CORBA_char     *dir,
		 CORBA_Environment    *ev)
{
	BonoboConfigArchiver *archiver_db = BONOBO_CONFIG_ARCHIVER (db);

	if (lookup_dir (archiver_db->dir, dir, FALSE))
		return TRUE;

	return FALSE;
}

static void
delete_dir_entry (DirEntry *de)
{
	CORBA_free (de->value);

	if (de->node) {
		xmlUnlinkNode (de->node);
		xmlFreeNode (de->node);
	}
	
	g_free (de->name);
	g_free (de);
}

static void
real_remove_value (BonoboConfigDatabase *db,
		   const CORBA_char     *key, 
		   CORBA_Environment    *ev)
{
	BonoboConfigArchiver *archiver_db = BONOBO_CONFIG_ARCHIVER (db);
	DirEntry *de;

	if (!(de = lookup_dir_entry (archiver_db, key, FALSE)))
		return;

	de->dir->entries = g_slist_remove (de->dir->entries, de);

	delete_dir_entry (de);
}

static void
delete_dir_data (DirData *dd, gboolean is_root)
{
	GSList *l;

	for (l = dd->subdirs; l; l = l->next)
		delete_dir_data ((DirData *)l->data, FALSE);

	g_slist_free (dd->subdirs);

	dd->subdirs = NULL;

        for (l = dd->entries; l; l = l->next) 
		delete_dir_entry ((DirEntry *)l->data);

	g_slist_free (dd->entries);

	dd->entries = NULL;

	if (!is_root) {
		g_free (dd->name);

		if (dd->node) {
			xmlUnlinkNode (dd->node);
			xmlFreeNode (dd->node);
		}
	
		g_free (dd);
	}
}

static void
real_remove_dir (BonoboConfigDatabase *db,
		 const CORBA_char     *dir, 
		 CORBA_Environment    *ev)
{
	BonoboConfigArchiver *archiver_db = BONOBO_CONFIG_ARCHIVER (db);
	DirData *dd;

	if (!(dd = lookup_dir (archiver_db->dir, dir, FALSE)))
		return;

	if (dd != archiver_db->dir && dd->dir)
		dd->dir->subdirs = g_slist_remove (dd->dir->subdirs, dd);

	delete_dir_data (dd, dd == archiver_db->dir);
}

static void
pb_get_fn (BonoboPropertyBag *bag, BonoboArg *arg,
	   guint arg_id, CORBA_Environment *ev,
	   gpointer user_data)
{
	BonoboConfigArchiver *archiver_db = BONOBO_CONFIG_ARCHIVER (user_data);
	time_t val;

	val = ConfigArchiver_Location_getModificationTime
		(archiver_db->location, archiver_db->backend_id, ev);

	BONOBO_ARG_SET_GENERAL (arg, val, TC_ulonglong, CORBA_unsigned_long_long, NULL);
}

static void
pb_set_fn (BonoboPropertyBag *bag, const BonoboArg *arg,
	   guint arg_id, CORBA_Environment *ev,
	   gpointer user_data)
{
	g_assert_not_reached ();
}

static void
bonobo_config_archiver_destroy (GtkObject *object)
{
	BonoboConfigArchiver *archiver_db = BONOBO_CONFIG_ARCHIVER (object);
	CORBA_Environment     ev;

	DEBUG_MSG ("Enter");

	gtk_object_ref (object);

	CORBA_exception_init (&ev);

	if (archiver_db->moniker != NULL) {
		bonobo_url_unregister ("BONOBO_CONF:ARCHIVER", archiver_db->moniker, &ev);
		g_free (archiver_db->moniker);

		if (BONOBO_EX (&ev)) {
			g_critical ("Could not unregister the archiver URL");
			CORBA_exception_init (&ev);
		}
	}

	if (archiver_db->listener_id != 0) {
		bonobo_event_source_client_remove_listener
			(archiver_db->location, archiver_db->listener_id, &ev);

		if (BONOBO_EX (&ev))
			g_critical ("Could not remove the rollback data listener");
	}

	CORBA_exception_free (&ev);

	if (archiver_db->doc != NULL) {
		delete_dir_data (archiver_db->dir, TRUE);
		archiver_db->dir = NULL;
		xmlFreeDoc (archiver_db->doc);
	}

	if (archiver_db->filename != NULL)
		g_free (archiver_db->filename);

	if (archiver_db->fp != NULL)
		fclose (archiver_db->fp);

	if (archiver_db->location != CORBA_OBJECT_NIL)
		bonobo_object_release_unref (archiver_db->location, NULL);

	if (archiver_db->archive != CORBA_OBJECT_NIL)
		bonobo_object_release_unref (archiver_db->archive, NULL);
	
	parent_class->destroy (object);

	gtk_object_unref (object);

	DEBUG_MSG ("Exit");
}


static void
bonobo_config_archiver_class_init (BonoboConfigDatabaseClass *class)
{
	GtkObjectClass *object_class = (GtkObjectClass *) class;
	BonoboConfigDatabaseClass *cd_class;

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = bonobo_config_archiver_destroy;

	cd_class = BONOBO_CONFIG_DATABASE_CLASS (class);
		
	cd_class->get_value    = real_get_value;
	cd_class->set_value    = real_set_value;
	cd_class->list_dirs    = real_list_dirs;
	cd_class->list_keys    = real_list_keys;
	cd_class->dir_exists   = real_dir_exists;
	cd_class->remove_value = real_remove_value;
	cd_class->remove_dir   = real_remove_dir;
	cd_class->sync         = real_sync;
}

static void
bonobo_config_archiver_init (BonoboConfigArchiver *archiver_db)
{
	archiver_db->dir = g_new0 (DirData, 1);

	/* This will always be writeable */
	BONOBO_CONFIG_DATABASE (archiver_db)->writeable = TRUE;
}

BONOBO_X_TYPE_FUNC (BonoboConfigArchiver, PARENT_TYPE, bonobo_config_archiver);

static void
read_section (DirData *dd)
{
	CORBA_Environment  ev;
	DirEntry          *de;
	xmlNodePtr         s;
	gchar             *name;

	CORBA_exception_init (&ev);

	s = dd->node->childs;

	while (s) {
		if (s->type == XML_ELEMENT_NODE && 
		    !strcmp (s->name, "entry") &&
		    (name = xmlGetProp(s, "name"))) {
			
			de = dir_lookup_entry (dd, name, TRUE);
			
			de->node = s;
			
			/* we only decode it if it is a single value, 
			 * multiple (localized) values are decoded in the
			 * get_value function */
			if (!(s->childs && s->childs->next))
				de->value = bonobo_config_xml_decode_any 
					((BonoboUINode *)s, NULL, &ev);
			
			xmlFree (name);
		}
		s = s->next;
	}

	CORBA_exception_free (&ev);
}

static void
fill_cache (BonoboConfigArchiver *archiver_db)
{
	xmlNodePtr  n;
	gchar      *path;
	DirData    *dd;

	n = archiver_db->doc->root->childs;

	while (n) {
		if (n->type == XML_ELEMENT_NODE && 
		    !strcmp (n->name, "section")) {

			path = xmlGetProp(n, "path");

			if (!path || !strcmp (path, "")) {
				dd = archiver_db->dir;
			} else 
				dd = lookup_dir (archiver_db->dir, path, TRUE);
				
			if (!dd->node)
				dd->node = n;

			read_section (dd);
			
			xmlFree (path);
		}
		n = n->next;
	}
}

/* parse_name
 *
 * Given a moniker with a backend id and (possibly) a location id encoded
 * therein, parse out the backend id and the location id and set the pointers
 * given to them.
 *
 * FIXME: Is this encoding really the way we want to do this? Ask Dietmar and
 * Michael.
 */

static gboolean
parse_name (const gchar *name, gchar **backend_id, gchar **location, struct tm **date) 
{
	gchar *e, *e1, *time_str = NULL;

	*date = NULL;

	if (name[0] == '[') {
		e = strchr (name + 1, '|');

		if (e != NULL) {
			*location = g_strndup (name + 1, e - (name + 1));
			e1 = strchr (e + 1, ']');

			if (e1 != NULL) {
				time_str = g_strndup (e + 1, e1 - (e + 1));
				*date = parse_date (time_str);
				g_free (time_str);
			}

			*backend_id = g_strdup (e1 + 1);
		} else {
			e = strchr (name + 1, ']');

			if (e != NULL)
				*location = g_strndup (name + 1, e - (name + 1));
			else
				return FALSE;

			*backend_id = g_strdup (e + 1);
		}
	} else {
		*backend_id = g_strdup (name);
		*location = NULL;
	}

	if (*location != NULL && **location == '\0') {
		g_free (*location);
		*location = NULL;
	}

	return TRUE;
}

static void
new_rollback_cb (BonoboListener        *listener,
		 gchar                 *event_name,
		 CORBA_any             *any,
		 CORBA_Environment     *ev,
		 BonoboConfigArchiver  *archiver_db)
{
	BonoboArg *arg;

	if (archiver_db->is_up_to_date) {
		archiver_db->is_up_to_date = FALSE;
		return;
	}

	if (archiver_db->dir != NULL) {
		delete_dir_data (archiver_db->dir, TRUE);
		g_free (archiver_db->dir->name);
		g_free (archiver_db->dir);
		archiver_db->dir = g_new0 (DirData, 1);
	}

	if (archiver_db->doc != NULL)
		xmlFreeDoc (archiver_db->doc);

	archiver_db->doc = location_client_load_rollback_data
		(archiver_db->location, NULL, 0, archiver_db->backend_id, TRUE, ev);

	if (archiver_db->doc == NULL)
		g_critical ("Could not load new rollback data");
	else
		fill_cache (archiver_db);

	arg = bonobo_arg_new (BONOBO_ARG_NULL);
	bonobo_event_source_notify_listeners (archiver_db->es, "Bonobo/ConfigDatabase:sync", arg, ev);
	bonobo_arg_release (arg);
}

Bonobo_ConfigDatabase
bonobo_config_archiver_new (Bonobo_Moniker               parent,
			    const Bonobo_ResolveOptions *options,
			    const char                  *moniker,
			    CORBA_Environment           *ev)
{
	BonoboConfigArchiver    *archiver_db;
	Bonobo_ConfigDatabase    db;
	ConfigArchiver_Archive   archive;
	ConfigArchiver_Location  location;
	gchar                   *moniker_tmp;
	gchar                   *backend_id;
	gchar                   *location_id;
	struct tm               *date;

	g_return_val_if_fail (backend_id != NULL, NULL);

	/* Check the Bonobo URL database to see if this archiver database has
	 * already been created, and return it if it has */

	moniker_tmp = g_strdup (moniker);
	db = bonobo_url_lookup ("BONOBO_CONF:ARCHIVER", moniker_tmp, ev);
	g_free (moniker_tmp);

	if (BONOBO_EX (ev)) {
		db = CORBA_OBJECT_NIL;
		CORBA_exception_init (ev);
	}

	if (db != CORBA_OBJECT_NIL)
		return bonobo_object_dup_ref (db, NULL);

	/* Parse out the backend id, location id, and rollback date from the
	 * moniker given */

	if (parse_name (moniker, &backend_id, &location_id, &date) < 0) {
		EX_SET_NOT_FOUND (ev);
		return CORBA_OBJECT_NIL;
	}

	/* Resolve the parent archive and the location */

	archive = Bonobo_Moniker_resolve (parent, options, "IDL:ConfigArchiver/Archive:1.0", ev);

	if (BONOBO_EX (ev) || archive == CORBA_OBJECT_NIL) {
		g_free (location_id);
		g_free (date);
		return CORBA_OBJECT_NIL;
	}

	if (location_id == NULL || *location_id == '\0')
		location = ConfigArchiver_Archive__get_currentLocation (archive, ev);
	else
		location = ConfigArchiver_Archive_getLocation (archive, location_id, ev);

	g_free (location_id);

	if (location == CORBA_OBJECT_NIL) {
		g_free (date);
		bonobo_object_release_unref (archive, NULL);
		return CORBA_OBJECT_NIL;
	}

	/* Construct the database object proper and fill in its values */

	if ((archiver_db = gtk_type_new (BONOBO_CONFIG_ARCHIVER_TYPE)) == NULL) {
		g_free (date);
		return CORBA_OBJECT_NIL;
	}

	archiver_db->backend_id = backend_id;
	archiver_db->moniker    = g_strdup (moniker);
	archiver_db->archive    = archive;
	archiver_db->location   = location;

	/* Load the XML data, or use the defaults file if none are present */

	archiver_db->doc = location_client_load_rollback_data
		(archiver_db->location, date, 0, archiver_db->backend_id, TRUE, ev);
	g_free (date);

	if (BONOBO_EX (ev) || archiver_db->doc == NULL) {
		gchar *filename;

		filename = g_strconcat (GNOMECC_DEFAULTS_DIR "/", archiver_db->backend_id, ".xml", NULL);
		archiver_db->doc = xmlParseFile (filename);
		g_free (filename);

		if (archiver_db->doc == NULL) {
			bonobo_object_release_unref (archiver_db->location, NULL);
			bonobo_object_release_unref (archiver_db->archive, NULL);
			bonobo_object_unref (BONOBO_OBJECT (archiver_db));
			return CORBA_OBJECT_NIL;
		}

		CORBA_exception_init (ev);
	}

	/* Load data from the XML file */

	if (archiver_db->doc->root == NULL)
		archiver_db->doc->root = 
			xmlNewDocNode (archiver_db->doc, NULL, "bonobo-config", NULL);

	if (strcmp (archiver_db->doc->root->name, "bonobo-config")) {
		xmlFreeDoc (archiver_db->doc);
		archiver_db->doc = xmlNewDoc("1.0");
		archiver_db->doc->root = 
			xmlNewDocNode (archiver_db->doc, NULL, "bonobo-config", NULL);
	}

	fill_cache (archiver_db);

	/* Construct the associated property bag and event source */

#if 0
	archiver_db->es = bonobo_event_source_new ();

	bonobo_object_add_interface (BONOBO_OBJECT (archiver_db), 
				     BONOBO_OBJECT (archiver_db->es));
#endif
	
	archiver_db->pb = bonobo_property_bag_new (pb_get_fn,
						   pb_set_fn,
						   archiver_db);

	bonobo_object_add_interface (BONOBO_OBJECT (archiver_db), 
				     BONOBO_OBJECT (archiver_db->pb));

	archiver_db->es = archiver_db->pb->es; 
		
	bonobo_property_bag_add (archiver_db->pb,
				 "last_modified", 1, TC_ulonglong, NULL,
		       		 "Date (time_t) of modification", 
				 BONOBO_PROPERTY_READABLE);

	/* Listen for events pertaining to new rollback data */

	if (date == NULL && location_id == NULL)
		archiver_db->listener_id =
			bonobo_event_source_client_add_listener
			(location, (BonoboListenerCallbackFn) new_rollback_cb,
			 "ConfigArchiver/Location:newRollbackData", ev, archiver_db);
	else
		archiver_db->listener_id = 0;

	/* Prepare to return the database object */

	db = CORBA_Object_duplicate (BONOBO_OBJREF (archiver_db), NULL);

	moniker_tmp = g_strdup (moniker);
	bonobo_url_register ("BONOBO_CONF:ARCHIVER", moniker_tmp, NULL, db, ev);
	g_free (moniker_tmp);

	return db;
}
