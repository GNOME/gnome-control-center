/* -*- mode: c; style: linux -*- */

/* archiver-client.c
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Written by Bradford Hovinen (hovinen@ximian.com)
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
#  include <config.h>
#endif

#include <libbonobo.h>
#include <gnome-xml/parser.h>

#include "archiver-client.h"
#include "util.h"

static void merge_xml_docs           (xmlDocPtr child_doc,
				      xmlDocPtr parent_doc);
static void subtract_xml_doc         (xmlDocPtr child_doc,
				      xmlDocPtr parent_doc,
				      gboolean strict);
static void merge_xml_nodes          (xmlNodePtr node1,
				      xmlNodePtr node2);
static xmlNodePtr subtract_xml_node  (xmlNodePtr node1,
				      xmlNodePtr node2,
				      gboolean strict);
static gboolean compare_xml_nodes    (xmlNodePtr node1, xmlNodePtr node2);

/**
 * location_client_load_rollback_data
 * @location:
 * @date:
 * @steps:
 * @backend_id:
 * @parent_chain:
 *
 * Loads the XML data for rolling back as specified and returns the document
 * object
 **/

xmlDocPtr
location_client_load_rollback_data (ConfigArchiver_Location  location,
				    const struct tm         *date,
				    guint                    steps,
				    const gchar             *backend_id,
				    gboolean                 parent_chain,
				    CORBA_Environment       *opt_ev)
{
	gchar                          *filename;
	struct tm                      *date_c;
	time_t                          time_g;

	xmlDocPtr                       doc        = NULL;
	xmlDocPtr                       parent_doc = NULL;
	ConfigArchiver_ContainmentType  type       = ConfigArchiver_CONTAIN_FULL;
	ConfigArchiver_Location         parent     = CORBA_OBJECT_NIL;

	CORBA_Environment               my_ev;

	g_return_val_if_fail (location != CORBA_OBJECT_NIL, NULL);

	if (opt_ev == NULL) {
		opt_ev = &my_ev;
		CORBA_exception_init (opt_ev);
	}

	if (date != NULL) {
		date_c = dup_date (date);
		time_g = mktime (date_c);
#ifdef __USE_BSD
		time_g += date_c->tm_gmtoff;
#endif /* __USE_BSD */
		if (date_c->tm_isdst) time_g -= 3600;
		g_free (date_c);
	} else {
		time_g = 0;
	}

	filename = ConfigArchiver_Location_getRollbackFilename
		(location, time_g, steps, backend_id, parent_chain, opt_ev);

	if (!BONOBO_EX (opt_ev) && filename != NULL)
		DEBUG_MSG ("Loading rollback data: %s", filename);

	if (!BONOBO_EX (opt_ev) && filename != NULL)
		doc = xmlParseFile (filename);
	else if (parent_chain)
		type = ConfigArchiver_Location_contains (location, backend_id, opt_ev);

	if (type == ConfigArchiver_CONTAIN_PARTIAL)
		parent = ConfigArchiver_Location__get_parent (location, opt_ev);

	if (parent != CORBA_OBJECT_NIL) {
		parent_doc = location_client_load_rollback_data
			(parent, date, steps, backend_id, TRUE, opt_ev);
		bonobo_object_release_unref (parent, NULL);
	}

	if (doc != NULL && parent_doc != NULL)
		merge_xml_docs (doc, parent_doc);
	else if (parent_doc != NULL)
		doc = parent_doc;

	if (opt_ev == &my_ev)
		CORBA_exception_free (opt_ev);

	return doc;
}

/**
 * location_client_store_xml:
 * @location: 
 * @backend_id: 
 * @input: 
 * @store_type: STORE_FULL means blindly store the data, without
 * modification. STORE_COMPARE_PARENT means subtract the settings the parent
 * has that are different and store the result. STORE_MASK_PREVIOUS means
 * store only those settings that are reflected in the previous logged data;
 * if there do not exist such data, act as in STORE_COMPARE_PARENT
 * @opt_ev:
 * 
 * Store configuration data from the given XML document object in the location
 * under the given backend id
 **/

void 
location_client_store_xml (ConfigArchiver_Location   location,
			   const gchar              *backend_id,
			   xmlDocPtr                 xml_doc,
			   ConfigArchiver_StoreType  store_type,
			   CORBA_Environment        *opt_ev) 
{
	xmlDocPtr                       parent_doc;
	xmlDocPtr                       prev_doc = NULL;
	char                           *filename;
	ConfigArchiver_ContainmentType  contain_type;
	ConfigArchiver_Location         parent;
	CORBA_Environment               my_ev;

	g_return_if_fail (location != CORBA_OBJECT_NIL);
	g_return_if_fail (xml_doc != NULL);

	if (opt_ev == NULL) {
		opt_ev = &my_ev;
		CORBA_exception_init (opt_ev);
	}

	contain_type = ConfigArchiver_Location_contains (location, backend_id, opt_ev);
	parent = ConfigArchiver_Location__get_parent (location, opt_ev);

	if (contain_type == ConfigArchiver_CONTAIN_NONE) {
		if (parent == CORBA_OBJECT_NIL) {
			fprintf (stderr, "Could not find a location in the " \
				 "tree ancestry that stores this " \
				 "backend: %s.\n", backend_id);
		} else {
			location_client_store_xml (parent, backend_id, xml_doc, store_type, opt_ev);
			bonobo_object_release_unref (parent, NULL);
		}

		if (opt_ev == &my_ev)
			CORBA_exception_free (opt_ev);

		return;
	}

	if (contain_type == ConfigArchiver_CONTAIN_PARTIAL && store_type != ConfigArchiver_STORE_FULL && parent != CORBA_OBJECT_NIL) {
		g_assert (store_type == ConfigArchiver_STORE_MASK_PREVIOUS ||
			  store_type == ConfigArchiver_STORE_COMPARE_PARENT);

		parent_doc = location_client_load_rollback_data
			(parent, NULL, 0, backend_id, TRUE, opt_ev);

		if (store_type == ConfigArchiver_STORE_MASK_PREVIOUS)
			prev_doc = location_client_load_rollback_data (location, NULL, 0, backend_id, FALSE, opt_ev);

		if (store_type == ConfigArchiver_STORE_COMPARE_PARENT) {
			subtract_xml_doc (xml_doc, parent_doc, FALSE);
		} else {
			subtract_xml_doc (parent_doc, prev_doc, FALSE);
			subtract_xml_doc (xml_doc, parent_doc, TRUE);
		}

		xmlFreeDoc (parent_doc);

		if (prev_doc != NULL)
			xmlFreeDoc (prev_doc);
	}

	if (parent != CORBA_OBJECT_NIL)
		bonobo_object_release_unref (parent, NULL);

	filename = ConfigArchiver_Location_getStorageFilename
		(location, backend_id, store_type == ConfigArchiver_STORE_DEFAULT, opt_ev);

	if (!BONOBO_EX (opt_ev) && filename != NULL) {
		xmlSaveFile (filename, xml_doc);
		ConfigArchiver_Location_storageComplete (location, filename, opt_ev);
		CORBA_free (filename);
	}

	if (opt_ev == &my_ev)
		CORBA_exception_free (opt_ev);
}

static void
merge_xml_docs (xmlDocPtr child_doc, xmlDocPtr parent_doc)
{
	merge_xml_nodes (xmlDocGetRootElement (child_doc),
			 xmlDocGetRootElement (parent_doc));
}

static void
subtract_xml_doc (xmlDocPtr child_doc, xmlDocPtr parent_doc, gboolean strict) 
{
	subtract_xml_node (xmlDocGetRootElement (child_doc),
			   xmlDocGetRootElement (parent_doc), strict);
}

/* Merge contents of node1 and node2, where node1 overrides node2 as
 * appropriate
 *
 * Notes: Two XML nodes are considered to be "the same" iff their names and
 * all names and values of their attributes are the same. If that is not the
 * case, they are considered "different" and will both be present in the
 * merged node. If nodes are "the same", then this algorithm is called
 * recursively. If nodes are CDATA, then node1 overrides node2 and the
 * resulting node is just node1. The node merging is order-independent; child
 * node from one tree are compared with all child nodes of the other tree
 * regardless of the order they appear in. Hence one may have documents with
 * different node orderings and the algorithm should still run correctly. It
 * will not, however, run correctly in cases when the agent using this
 * facility depends on the nodes being in a particular order.
 *
 * This XML node merging/comparison facility requires that the following
 * standard be set for DTDs:
 *
 * Attributes' sole purpose is to identify a node. For example, a network
 * configuration DTD might have an element like <interface name="eth0"> with a
 * bunch of child nodes to configure that interface. The attribute "name" does
 * not specify the configuration for the interface. It differentiates the node
 * from the configuration for, say, interface eth1. Conversely, a node must be
 * completely identified by its attributes. One cannot include identification
 * information in the node's children, since otherwise the merging and
 * subtraction algorithms will not know what to look for.
 *
 * As a corollary to the above, all configuration information must ultimately
 * be in text nodes. For example, a text string might be stored as
 * <configuration-item>my-value</configuration-item> but never as
 * <configuration-item value="my-value"/>. As an example, if the latter is
 * used, a child location might override a parent's setting for
 * configuration-item. This algorithm will interpret those as different nodes
 * and include them both in the merged result, since it will not have any way
 * of knowing that they are really the same node with different
 * configuration.
 */

static void
merge_xml_nodes (xmlNodePtr node1, xmlNodePtr node2) 
{
	xmlNodePtr child, tmp, iref;
	GList *node1_children = NULL, *i;
	gboolean found;

	if (node1->type == XML_TEXT_NODE)
		return;

	for (child = node1->childs; child != NULL; child = child->next)
		node1_children = g_list_prepend (node1_children, child);

	node1_children = g_list_reverse (node1_children);

	child = node2->childs;

	while (child != NULL) {
		tmp = child->next;

		i = node1_children; found = FALSE;

		while (i != NULL) {
			iref = (xmlNodePtr) i->data;

			if (compare_xml_nodes (iref, child)) {
				merge_xml_nodes (iref, child);
				if (i == node1_children)
					node1_children = node1_children->next;
				g_list_remove_link (node1_children, i);
				g_list_free_1 (i);
				found = TRUE;
				break;
			} else {
				i = i->next;
			}
		}

		if (found == FALSE) {
			xmlUnlinkNode (child);
			xmlAddChild (node1, child);
		}

		child = tmp;
	}

	g_list_free (node1_children);
}

/* Modifies node1 so that it only contains the parts different from node2;
 * returns the modified node or NULL if the node should be destroyed
 *
 * strict determines whether the settings themselves are compared; it should
 * be set to FALSE when the trees are being compared for the purpose of
 * seeing what settings should be included in a tree and TRUE when one wants
 * to restrict the settings included in a tree to those that have already been
 * specified
 */

static xmlNodePtr
subtract_xml_node (xmlNodePtr node1, xmlNodePtr node2, gboolean strict) 
{
	xmlNodePtr child, tmp, iref;
	GList *node2_children = NULL, *i;
	gboolean found, same, all_same = TRUE;

	if (node1->type == XML_TEXT_NODE) {
		if (node2->type == XML_TEXT_NODE &&
		    (strict || !strcmp (xmlNodeGetContent (node1),
					xmlNodeGetContent (node2))))
			return NULL;
		else
			return node1;
	}

	if (node1->childs == NULL && node2->childs == NULL)
		return NULL;

	for (child = node2->childs; child != NULL; child = child->next)
		node2_children = g_list_prepend (node2_children, child);

	node2_children = g_list_reverse (node2_children);

	child = node1->childs;

	while (child != NULL) {
		tmp = child->next;
		i = node2_children; found = FALSE; all_same = TRUE;

		while (i != NULL) {
			iref = (xmlNodePtr) i->data;

			if (compare_xml_nodes (child, iref)) {
				same = (subtract_xml_node
					(child, iref, strict) == NULL);
				all_same = all_same && same;

				if (same) {
					xmlUnlinkNode (child);
					xmlFreeNode (child);
				}

				if (i == node2_children)
					node2_children = node2_children->next;
				g_list_remove_link (node2_children, i);
				g_list_free_1 (i);
				found = TRUE;
				break;
			} else {
				i = i->next;
			}
		}

		if (!found)
			all_same = FALSE;

		child = tmp;
	}

	g_list_free (node2_children);

	if (all_same)
		return NULL;
	else
		return node1;
}

/* Return TRUE iff node1 and node2 are "the same" in the sense defined above */

static gboolean
compare_xml_nodes (xmlNodePtr node1, xmlNodePtr node2) 
{
	xmlAttrPtr attr;
	gint count = 0;

	if (strcmp (node1->name, node2->name))
		return FALSE;

	/* FIXME: This is worst case O(n^2), which can add up. Could we
	 * optimize for the case where people have attributes in the same
	 * order, or does not not matter? It probably does not matter, though,
	 * since people don't generally have more than one or two attributes
	 * in a tag anyway.
	 */

	for (attr = node1->properties; attr != NULL; attr = attr->next) {
		g_assert (xmlNodeIsText (attr->val));

		if (strcmp (xmlNodeGetContent (attr->val),
			    xmlGetProp (node2, attr->name)))
			return FALSE;

		count++;
	}

	/* FIXME: Is checking if the two nodes have the same number of
	 * attributes the correct policy here? Should we instead merge the
	 * attribute(s) that node1 is missing?
	 */

	for (attr = node2->properties; attr != NULL; attr = attr->next)
		count--;

	if (count == 0)
		return TRUE;
	else
		return FALSE;
}
