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
#include "mime-types-model.h"

ModelEntry *
get_model_entries (GtkTreeModel *model)
{
	static ModelEntry *root = NULL;

	if (root == NULL) {
		root = g_new0 (ModelEntry, 1);
		root->type = MODEL_ENTRY_NONE;

		load_all_mime_types (model);
		load_all_services (model);
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
model_entry_insert_child (ModelEntry *parent, ModelEntry *child, GtkTreeModel *model)
{
	ModelEntry **tmp;
	GtkTreePath *path;
	GtkTreeIter iter;

	g_return_if_fail (parent != NULL);
	g_return_if_fail (parent->type == MODEL_ENTRY_CATEGORY || parent->type == MODEL_ENTRY_SERVICES_CATEGORY ||
			  parent->type == MODEL_ENTRY_NONE);
	g_return_if_fail (child != NULL);
	g_return_if_fail (model != NULL);
	g_return_if_fail (IS_MIME_TYPES_MODEL (model));

	for (tmp = &parent->first_child; *tmp != NULL; tmp = &((*tmp)->next)) {
		if ((*tmp)->type < child->type)
			continue;
		if ((*tmp)->type > child->type ||
		    (child->type == MODEL_ENTRY_CATEGORY &&
		     strcmp (MIME_CATEGORY_INFO (child)->name,
			     MIME_CATEGORY_INFO (*tmp)->name) < 0))
				break;
	}

	child->parent = parent;
	child->next = *tmp;
	*tmp = child;

	mime_types_model_construct_iter (MIME_TYPES_MODEL (model), child, &iter);
	path = gtk_tree_model_get_path (model, &iter);
	gtk_tree_model_row_inserted (model, path, &iter);
	gtk_tree_path_free (path);
}

void
model_entry_remove_child (ModelEntry *entry, ModelEntry *child, GtkTreeModel *model)
{
	ModelEntry *tmp;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean found = TRUE;

	g_return_if_fail (entry != NULL);
	g_return_if_fail (entry->type == MODEL_ENTRY_CATEGORY || entry->type == MODEL_ENTRY_SERVICES_CATEGORY ||
			  entry->type == MODEL_ENTRY_NONE);
	g_return_if_fail (child != NULL);
	g_return_if_fail (model != NULL);
	g_return_if_fail (IS_MIME_TYPES_MODEL (model));

	mime_types_model_construct_iter (MIME_TYPES_MODEL (model), child, &iter);
	path = gtk_tree_model_get_path (model, &iter);

	if (entry->first_child == NULL) {
		return;
	}
	else if (entry->first_child == child) {
		entry->first_child = child->next;
	} else {
		for (tmp = entry->first_child; tmp->next != NULL && tmp->next != child; tmp = tmp->next);

		if (tmp->next != NULL)
			tmp->next = child->next;
		else
			found = FALSE;
	}

	child->parent = NULL;

	if (found)
		gtk_tree_model_row_deleted (model, path);

	gtk_tree_path_free (path);
}

void
model_entry_save (ModelEntry *entry) 
{
	gnome_vfs_mime_freeze ();
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
	gnome_vfs_mime_thaw ();
}

void
model_entry_delete (ModelEntry *entry) 
{
	switch (entry->type) {
	case MODEL_ENTRY_MIME_TYPE:
		gnome_vfs_mime_registered_mime_type_delete (MIME_TYPE_INFO (entry)->mime_type);
		mime_type_info_free (MIME_TYPE_INFO (entry));
		break;

	case MODEL_ENTRY_SERVICE:
		service_info_delete (SERVICE_INFO (entry));
		service_info_free (SERVICE_INFO (entry));
		break;

	default:
		break;
	}
}
