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

static GtkObjectClass *parent_class;

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

guint
backend_list_get_type (void)
{
	static guint backend_list_type = 0;

	if (!backend_list_type) {
		GtkTypeInfo backend_list_info = {
			"BackendList",
			sizeof (BackendList),
			sizeof (BackendListClass),
			(GtkClassInitFunc) backend_list_class_init,
			(GtkObjectInitFunc) backend_list_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		backend_list_type = 
			gtk_type_unique (gtk_object_get_type (), 
					 &backend_list_info);
	}

	return backend_list_type;
}

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

	parent_class = GTK_OBJECT_CLASS
		(gtk_type_class (gtk_object_get_type ()));
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

GtkObject *
backend_list_new (gboolean is_global) 
{
	return gtk_object_new (backend_list_get_type (),
			       "is-global", is_global,
			       NULL);
}

gboolean
backend_list_contains (BackendList *backend_list, gchar *backend_id)
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
backend_list_add (BackendList *backend_list, gchar *backend_id)
{
	g_return_if_fail (backend_list != NULL);
	g_return_if_fail (IS_BACKEND_LIST (backend_list));
	g_return_if_fail (backend_id != NULL);

	backend_list->p->backend_ids =
		g_list_prepend (backend_list->p->backend_ids, backend_id);
}

void
backend_list_remove (BackendList *backend_list, gchar *backend_id)
{
	g_return_if_fail (backend_list != NULL);
	g_return_if_fail (IS_BACKEND_LIST (backend_list));
	g_return_if_fail (backend_id != NULL);

	backend_list->p->backend_ids = 
		g_list_remove (backend_list->p->backend_ids, backend_id);
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
