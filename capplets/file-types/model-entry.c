/* -*- mode: c; style: linux -*- */

/* model-entry.c
 *
 * Copyright (C) 2002 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "model-entry.h"
#include "mime-type-info.h"
#include "service-info.h"

/* List of MimeTypeInfo structures that have data to be committed */

static GList *dirty_list = NULL;

ModelEntry *
get_model_entries (void)
{
	static ModelEntry *root;

	if (root == NULL) {
		root = g_new0 (ModelEntry, 1);
		root->type = MODEL_ENTRY_NONE;

		load_all_mime_types ();
		load_all_services ();
	}

	return root;
}

ModelEntry *
model_entry_get_nth_child (ModelEntry *entry, gint n, gboolean categories_only)
{
	ModelEntry *tmp;

	g_return_val_if_fail (entry != NULL, NULL);
	g_return_val_if_fail (entry->type == MODEL_ENTRY_CATEGORY || entry->type == MODEL_ENTRY_SERVICES_CATEGORY ||
			      entry->type == MODEL_ENTRY_NONE, NULL);

	for (tmp = entry->first_child; tmp != NULL; tmp = tmp->next) {
		if (categories_only && tmp->type != MODEL_ENTRY_CATEGORY)
			continue;

		if (n-- == 0)
			break;
	}

	return tmp;
}

gint
model_entry_get_index (ModelEntry *parent, ModelEntry *child)
{
	ModelEntry *tmp;
	gint i = 0;

	g_return_val_if_fail (parent != NULL, -1);
	g_return_val_if_fail (parent->type == MODEL_ENTRY_CATEGORY || parent->type == MODEL_ENTRY_SERVICES_CATEGORY ||
			      parent->type == MODEL_ENTRY_NONE, -1);
	g_return_val_if_fail (child != NULL, -1);

	for (tmp = parent->first_child; tmp != NULL && tmp != child; tmp = tmp->next)
		i++;

	if (tmp == child)
		return i;
	else
		return -1;
}

void
model_entry_insert_child (ModelEntry *entry, ModelEntry *child)
{
	ModelEntry *tmp;

	g_return_if_fail (entry != NULL);
	g_return_if_fail (entry->type == MODEL_ENTRY_CATEGORY || entry->type == MODEL_ENTRY_SERVICES_CATEGORY ||
			  entry->type == MODEL_ENTRY_NONE);
	g_return_if_fail (child != NULL);

	if (entry->first_child == NULL) {
		entry->first_child = child;
	} else {
		for (tmp = entry->first_child; tmp->next != NULL; tmp = tmp->next);
		tmp->next = child;
	}
}

void
model_entry_remove_child (ModelEntry *entry, ModelEntry *child)
{
	ModelEntry *tmp;

	g_return_if_fail (entry != NULL);
	g_return_if_fail (entry->type == MODEL_ENTRY_CATEGORY || entry->type == MODEL_ENTRY_SERVICES_CATEGORY ||
			  entry->type == MODEL_ENTRY_NONE);
	g_return_if_fail (child != NULL);

	if (entry->first_child == NULL) {
		return;
	}
	else if (entry->first_child == child) {
		entry->first_child = child->next;
	} else {
		for (tmp = entry->first_child; tmp->next != NULL && tmp->next != child; tmp = tmp->next);
		tmp->next = child->next;
	}
}

void
model_entry_save (ModelEntry *entry) 
{
	switch (entry->type) {
	case MODEL_ENTRY_MIME_TYPE:
		mime_type_info_save (MIME_TYPE_INFO (entry));
		break;

	case MODEL_ENTRY_SERVICE:
		service_info_save (SERVICE_INFO (entry));
		break;

	case MODEL_ENTRY_CATEGORY:
		mime_category_info_save (MIME_CATEGORY_INFO (entry));
		break;

	default:
		break;
	}
}

void
model_entry_append_to_dirty_list (ModelEntry *entry)
{
	if (g_list_find (dirty_list, entry) == NULL)
		dirty_list = g_list_prepend (dirty_list, entry);
}

void
model_entry_remove_from_dirty_list (ModelEntry *entry)
{
	dirty_list = g_list_remove (dirty_list, entry);
}

void
model_entry_commit_dirty_list (void)
{
	gnome_vfs_mime_freeze ();
	g_list_foreach (dirty_list, (GFunc) model_entry_save, NULL);
	gnome_vfs_mime_thaw ();
}
