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
#include <bonobo/bonobo-arg.h>
#include <bonobo/bonobo-property-bag-xml.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo-conf/bonobo-config-utils.h>
#include <gnome-xml/xmlmemory.h>
#include <gtk/gtkmain.h>

#include "bonobo-config-archiver.h"

static GtkObjectClass *parent_class = NULL;

#define CLASS(o) BONOBO_CONFIG_ARCHIVER_CLASS (GTK_OBJECT(o)->klass)

#define PARENT_TYPE BONOBO_CONFIG_DATABASE_TYPE
#define FLUSH_INTERVAL 30 /* 30 seconds */

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

static void
real_sync (BonoboConfigDatabase *db, 
	   CORBA_Environment    *ev)
{
	BonoboConfigArchiver *archiver_db = BONOBO_CONFIG_ARCHIVER (db);

	if (!db->writeable)
		return;

	location_store_xml (archiver_db->location, archiver_db->backend_id, 
			    archiver_db->doc, STORE_MASK_PREVIOUS);
}

static gint
timeout_cb (gpointer data)
{
	BonoboConfigArchiver *archiver_db = BONOBO_CONFIG_ARCHIVER (data);
	CORBA_Environment ev;

	CORBA_exception_init(&ev);

	real_sync (BONOBO_CONFIG_DATABASE (data), &ev);
	
	CORBA_exception_free (&ev);

	archiver_db->time_id = 0;

	/* remove the timeout */
	return 0;
}

static void
notify_listeners (BonoboConfigArchiver *archiver_db, 
		  const char        *key, 
		  const CORBA_any   *value)
{
	CORBA_Environment ev;
	char *dir_name;
	char *leaf_name;
	char *ename;

	if (!key)
		return;

	CORBA_exception_init(&ev);

	ename = g_strconcat ("Bonobo/Property:change:", key, NULL);

	bonobo_event_source_notify_listeners(archiver_db->es, ename, value, &ev);

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

/*  	if (!archiver_db->time_id) */
/*  		archiver_db->time_id = gtk_timeout_add (FLUSH_INTERVAL * 1000,  */
/*  						  (GtkFunction)timeout_cb,  */
/*  						  archiver_db); */

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
bonobo_config_archiver_destroy (GtkObject *object)
{
	BonoboConfigArchiver *archiver_db = BONOBO_CONFIG_ARCHIVER (object);
	CORBA_Environment     ev;

	CORBA_exception_init (&ev);

	if (archiver_db->real_name != NULL) {
		bonobo_url_unregister ("BONOBO_CONF:ARCHIVER", archiver_db->real_name, &ev);
		g_free (archiver_db->real_name);
	}

	CORBA_exception_free (&ev);

	if (archiver_db->doc)
		xmlFreeDoc (archiver_db->doc);
	
	if (archiver_db->filename)
		g_free (archiver_db->filename);

	if (archiver_db->fp)
		fclose (archiver_db->fp);

	if (archiver_db->es)
		bonobo_object_unref (BONOBO_OBJECT (archiver_db->es));

	parent_class->destroy (object);
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

Bonobo_ConfigDatabase
bonobo_config_archiver_new (const char *backend_id, const char *location_id)
{
	BonoboConfigArchiver  *archiver_db;
	Bonobo_ConfigDatabase  db;
	CORBA_Environment      ev;
	gchar                 *real_name;
	Archive               *archive;

	static GtkObject      *ref_obj = NULL;

	g_return_val_if_fail (backend_id != NULL, NULL);

	CORBA_exception_init (&ev);

	if (location_id == NULL)
		real_name = g_strdup (backend_id);
	else
		real_name = g_strconcat ("[", location_id, "]", backend_id, NULL);

	db = bonobo_url_lookup ("BONOBO_CONF:ARCHIVER", real_name, &ev);

	if (BONOBO_EX (&ev))
		db = CORBA_OBJECT_NIL;
	
	CORBA_exception_free (&ev);

	if (db) {
		g_free (real_name);
		return bonobo_object_dup_ref (db, NULL);
	}

	if (!(archiver_db = gtk_type_new (BONOBO_CONFIG_ARCHIVER_TYPE))) {
		g_free (real_name);
		return CORBA_OBJECT_NIL;
	}

	archive = ARCHIVE (archive_load (FALSE));

	if (location_id == NULL || *location_id == '\0')
		archiver_db->location = archive_get_current_location (archive);
	else
		archiver_db->location = archive_get_location (archive, location_id);

	if (archiver_db->location == NULL) {
		bonobo_object_unref (BONOBO_OBJECT (archiver_db));
		return CORBA_OBJECT_NIL;
	}

	archiver_db->backend_id = g_strdup (backend_id);
	archiver_db->archive = archive;
	archiver_db->real_name = real_name;
	
	archiver_db->doc = location_load_rollback_data
		(archiver_db->location, NULL, 0, archiver_db->backend_id, TRUE);

	if (archiver_db->doc == NULL) {
		gchar *filename;

		filename = g_strconcat (DEFAULTS_DIR, "/", archiver_db->backend_id, ".xml", NULL);
		archiver_db->doc = xmlParseFile (filename);
		g_free (filename);

		if (archiver_db->doc == NULL) {
			bonobo_object_unref (BONOBO_OBJECT (archiver_db));
			return CORBA_OBJECT_NIL;
		}
	}

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

	archiver_db->es = bonobo_event_source_new ();

	bonobo_object_add_interface (BONOBO_OBJECT (archiver_db), 
				     BONOBO_OBJECT (archiver_db->es));

	db = CORBA_Object_duplicate (BONOBO_OBJREF (archiver_db), NULL);

	bonobo_url_register ("BONOBO_CONF:ARCHIVER", real_name, NULL, db, &ev);

	if (ref_obj == NULL) {
		ref_obj = gtk_object_new (gtk_object_get_type (), NULL);
		gtk_signal_connect (ref_obj, "destroy", GTK_SIGNAL_FUNC (gtk_main_quit), NULL);
	} else {
		gtk_object_ref (ref_obj);
	}

	gtk_signal_connect_object (GTK_OBJECT (archiver_db), "destroy",
				   GTK_SIGNAL_FUNC (gtk_object_unref), ref_obj);

	return db;
}
