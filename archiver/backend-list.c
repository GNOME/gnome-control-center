/* -*- mode: c; style: linux -*- */

/* backend-list.c
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>
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

#include <parser.h>
#include <tree.h>

#include "backend-list.h"

enum {
	ARG_0,
	ARG_IS_GLOBAL
};

struct _BackendListPrivate 
{
	gboolean  is_global;
	gchar    *filename;
	GList    *backend_ids;
};

#define BACKEND_LIST_FROM_SERVANT(servant) (BACKEND_LIST (bonobo_object_from_servant (servant)))

static BonoboXObjectClass *parent_class;

static void backend_list_init        (BackendList *backend_list);
static void backend_list_class_init  (BackendListClass *class);

static void backend_list_set_arg     (GtkObject *object, 
				      GtkArg *arg, 
				      guint arg_id);
static void backend_list_get_arg     (GtkObject *object, 
				      GtkArg *arg, 
				      guint arg_id);

static void backend_list_finalize    (GtkObject *object);

static void do_load                  (BackendList *backend_list);
static void do_save                  (BackendList *backend_list);

/* CORBA interface methods */

static CORBA_boolean
impl_ConfigArchiver_BackendList_contains (PortableServer_Servant  servant,
					  const CORBA_char       *backend_id,
					  CORBA_Environment      *ev) 
{
	return backend_list_contains (BACKEND_LIST_FROM_SERVANT (servant), backend_id);
}

static void
impl_ConfigArchiver_BackendList_add (PortableServer_Servant  servant,
				     const CORBA_char       *backend_id,
				     CORBA_Environment      *ev)
{
	backend_list_add (BACKEND_LIST_FROM_SERVANT (servant), backend_id);
}

static void
impl_ConfigArchiver_BackendList_remove (PortableServer_Servant  servant,
					const CORBA_char       *backend_id,
					CORBA_Environment      *ev) 
{
	backend_list_remove (BACKEND_LIST_FROM_SERVANT (servant), backend_id);
}

static void
impl_ConfigArchiver_BackendList_save (PortableServer_Servant  servant,
				      CORBA_Environment      *ev) 
{
	backend_list_save (BACKEND_LIST_FROM_SERVANT (servant));
}

static ConfigArchiver_StringSeq *
impl_ConfigArchiver_BackendList__get_backends (PortableServer_Servant  servant,
					       CORBA_Environment      *ev) 
{
	ConfigArchiver_StringSeq *seq;
	BackendList *backend_list = BACKEND_LIST_FROM_SERVANT (servant);
	guint i = 0;
	GList *node;

	g_return_val_if_fail (backend_list != NULL, NULL);
	g_return_val_if_fail (IS_BACKEND_LIST (backend_list), NULL);

	seq = ConfigArchiver_StringSeq__alloc ();
	seq->_length = g_list_length (backend_list->p->backend_ids);
	seq->_buffer = CORBA_sequence_CORBA_string_allocbuf (seq->_length);
	CORBA_sequence_set_release (seq, TRUE);

	for (node = backend_list->p->backend_ids; node != NULL; node = node->next)
		seq->_buffer[i++] = CORBA_string_dup (node->data);

	return seq;
}

BONOBO_X_TYPE_FUNC_FULL (BackendList, ConfigArchiver_BackendList, BONOBO_X_OBJECT_TYPE, backend_list);

static void
backend_list_init (BackendList *backend_list)
{
	backend_list->p = g_new0 (BackendListPrivate, 1);
}

static void
backend_list_class_init (BackendListClass *class) 
{
	GtkObjectClass *object_class;

	gtk_object_add_arg_type ("BackendList::is-global",
				 GTK_TYPE_INT,
				 GTK_ARG_CONSTRUCT_ONLY | GTK_ARG_READWRITE,
				 ARG_IS_GLOBAL);

	object_class = GTK_OBJECT_CLASS (class);
	object_class->finalize = backend_list_finalize;
	object_class->set_arg = backend_list_set_arg;
	object_class->get_arg = backend_list_get_arg;

	class->epv.contains      = impl_ConfigArchiver_BackendList_contains;
	class->epv.add           = impl_ConfigArchiver_BackendList_add;
	class->epv.remove        = impl_ConfigArchiver_BackendList_remove;
	class->epv.save          = impl_ConfigArchiver_BackendList_save;

	class->epv._get_backends = impl_ConfigArchiver_BackendList__get_backends;

	parent_class = BONOBO_X_OBJECT_CLASS
		(gtk_type_class (BONOBO_X_OBJECT_TYPE));
}

static void
backend_list_set_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	BackendList *backend_list;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_BACKEND_LIST (object));

	backend_list = BACKEND_LIST (object);

	switch (arg_id) {
	case ARG_IS_GLOBAL:
		backend_list->p->is_global = GTK_VALUE_INT (*arg);

		backend_list->p->filename = backend_list->p->is_global ?
			LOCATION_DIR "/default-global.xml" :
				LOCATION_DIR "/default-user.xml";
		do_load (backend_list);

		break;

	default:
		g_warning ("Bad argument set");
		break;
	}
}

static void
backend_list_get_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	BackendList *backend_list;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_BACKEND_LIST (object));

	backend_list = BACKEND_LIST (object);

	switch (arg_id) {
	case ARG_IS_GLOBAL:
		GTK_VALUE_INT (*arg) = backend_list->p->is_global;
		break;

	default:
		g_warning ("Bad argument get");
		break;
	}
}

static void
backend_list_finalize (GtkObject *object) 
{
	BackendList *backend_list;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_BACKEND_LIST (object));

	backend_list = BACKEND_LIST (object);

	g_list_foreach (backend_list->p->backend_ids, (GFunc) g_free, NULL);
	g_list_free (backend_list->p->backend_ids);
	g_free (backend_list->p);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

BonoboObject *
backend_list_new (gboolean is_global) 
{
	return BONOBO_OBJECT
		(gtk_object_new (backend_list_get_type (),
				 "is-global", is_global,
				 NULL));
}

gboolean
backend_list_contains (BackendList *backend_list, const gchar *backend_id)
{
	GList *node;

	g_return_val_if_fail (backend_list != NULL, FALSE);
	g_return_val_if_fail (IS_BACKEND_LIST (backend_list), FALSE);

	for (node = backend_list->p->backend_ids; node != NULL;
	     node = node->next)
		if (!strcmp (node->data, backend_id))
			return TRUE;

	return FALSE;
}

/**
 * backend_list_foreach:
 * @backend_list: 
 * @callback: 
 * @data: 
 * 
 * Iterates through all the backends, invoking the callback given and aborting
 * if any callback returns a nonzero value
 * 
 * Return value: TRUE iff no callback issued a nonzero value, FALSE otherwise
 **/

gboolean
backend_list_foreach (BackendList *backend_list, BackendCB callback,
		      gpointer data)
{
	GList *node;

	g_return_val_if_fail (backend_list != NULL, FALSE);
	g_return_val_if_fail (IS_BACKEND_LIST (backend_list), FALSE);
	g_return_val_if_fail (callback != NULL, FALSE);

	for (node = backend_list->p->backend_ids; node; node = node->next)
		if (callback (backend_list, node->data, data)) return FALSE;

	return TRUE;
}

void
backend_list_add (BackendList *backend_list, const gchar *backend_id)
{
	g_return_if_fail (backend_list != NULL);
	g_return_if_fail (IS_BACKEND_LIST (backend_list));
	g_return_if_fail (backend_id != NULL);

	backend_list->p->backend_ids =
		g_list_prepend (backend_list->p->backend_ids, g_strdup (backend_id));
}

void
backend_list_remove (BackendList *backend_list, const gchar *backend_id)
{
	gchar *tmp;
	GList *node;

	g_return_if_fail (backend_list != NULL);
	g_return_if_fail (IS_BACKEND_LIST (backend_list));
	g_return_if_fail (backend_id != NULL);

	tmp = g_strdup (backend_id);
	node = g_list_find (backend_list->p->backend_ids, tmp);
	backend_list->p->backend_ids = 
		g_list_remove_link (backend_list->p->backend_ids, node);
	g_free (node->data);
	g_free (tmp);
	g_list_free_1 (node);
}

void
backend_list_save (BackendList *backend_list)
{
	g_return_if_fail (backend_list != NULL);
	g_return_if_fail (IS_BACKEND_LIST (backend_list));

	do_save (backend_list);
}

static void
do_load (BackendList *backend_list) 
{
	xmlNodePtr root_node, node;
	xmlDocPtr doc;
	GList *list_tail = NULL;
	gchar *contains_str;

	doc = xmlParseFile (backend_list->p->filename);
	if (doc == NULL) return;
	root_node = xmlDocGetRootElement (doc);

	for (node = root_node->childs; node; node = node->next) {
		if (!strcmp (node->name, "contains")) {
			contains_str = xmlGetProp (node, "backend");

			if (contains_str != NULL) {
				contains_str = g_strdup (contains_str);
				list_tail = g_list_append (list_tail, 
							   contains_str);
				if (backend_list->p->backend_ids == NULL)
					backend_list->p->backend_ids =
						list_tail;
				else
					list_tail = list_tail->next;
			} else {
				g_warning ("Bad backends list: " \
					   "contains element with no " \
					   "backend attribute");
			}
		}
	}
}

static void
do_save (BackendList *backend_list) 
{
	xmlNodePtr root_node, child_node;
	xmlDocPtr doc;
	GList *node;

	doc = xmlNewDoc ("1.0");
	root_node = xmlNewDocNode (doc, NULL, "location", NULL);

	for (node = backend_list->p->backend_ids; node; node = node->next) {
		child_node = xmlNewChild (root_node, NULL, "contains", NULL);
		xmlNewProp (child_node, "backend", node->data);
	}

	xmlDocSetRootElement (doc, root_node);
	xmlSaveFile (backend_list->p->filename, doc);
	xmlFreeDoc (doc);
}
