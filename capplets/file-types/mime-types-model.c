/* -*- mode: c; style: linux -*- */

/* mime-types-model.c
 * Copyright (C) 2000 Red Hat, Inc.,
 *           (C) 2002 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>
 * Based on code by Jonathan Blandford <jrb@redhat.com>
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
 *
 * The model-related bootstrapping is lifted from gtk+/gtk/gtktreestore.c
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "mime-types-model.h"
#include "mime-type-info.h"
#include "service-info.h"

#define IS_CATEGORY(entry) (entry->type == MODEL_ENTRY_CATEGORY)

enum {
	PROP_0,
	PROP_CATEGORY_ONLY
};

struct _MimeTypesModelPrivate 
{
	gint     stamp;
	gboolean category_only;
};

static GObjectClass *parent_class;



static void mime_types_model_init            (MimeTypesModel *mime_types_model,
					      MimeTypesModelClass *class);
static void mime_types_model_class_init      (MimeTypesModelClass *class);
static void mime_types_model_base_init       (MimeTypesModelClass *class);

static void mime_types_model_tree_model_init (GtkTreeModelIface *iface);

static void mime_types_model_set_prop        (GObject      *object, 
					      guint         prop_id,
					      const GValue *value, 
					      GParamSpec   *pspec);
static void mime_types_model_get_prop        (GObject      *object,
					      guint         prop_id,
					      GValue       *value,
					      GParamSpec   *pspec);

static void mime_types_model_finalize        (GObject *object);

static guint        mime_types_model_get_flags       (GtkTreeModel      *tree_model);
static gint         mime_types_model_get_n_columns   (GtkTreeModel      *tree_model);
static GType        mime_types_model_get_column_type (GtkTreeModel      *tree_model,
						      gint               index);
static gboolean     mime_types_model_get_iter        (GtkTreeModel      *tree_model,
						      GtkTreeIter       *iter,
						      GtkTreePath       *path);
static GtkTreePath *mime_types_model_get_path        (GtkTreeModel      *tree_model,
						      GtkTreeIter       *iter);
static void         mime_types_model_get_value       (GtkTreeModel      *tree_model,
						      GtkTreeIter       *iter,
						      gint               column,
						      GValue            *value);
static gboolean     mime_types_model_iter_next       (GtkTreeModel      *tree_model,
						      GtkTreeIter       *iter);
static gboolean     mime_types_model_iter_children   (GtkTreeModel      *tree_model,
						      GtkTreeIter       *iter,
						      GtkTreeIter       *parent);
static gboolean     mime_types_model_iter_has_child  (GtkTreeModel      *tree_model,
						      GtkTreeIter       *iter);
static gint         mime_types_model_iter_n_children (GtkTreeModel      *tree_model,
						      GtkTreeIter       *iter);
static gboolean     mime_types_model_iter_nth_child  (GtkTreeModel      *tree_model,
						      GtkTreeIter       *iter,
						      GtkTreeIter       *parent,
						      gint               n);
static gboolean     mime_types_model_iter_parent     (GtkTreeModel      *tree_model,
						      GtkTreeIter       *iter,
						      GtkTreeIter       *child);



GType
mime_types_model_get_type (void)
{
	static GType mime_types_model_type = 0;

	if (!mime_types_model_type) {
		static const GTypeInfo mime_types_model_info = {
			sizeof (MimeTypesModelClass),
			(GBaseInitFunc) mime_types_model_base_init,
			NULL, /* GBaseFinalizeFunc */
			(GClassInitFunc) mime_types_model_class_init,
			NULL, /* GClassFinalizeFunc */
			NULL, /* user-supplied data */
			sizeof (MimeTypesModel),
			0, /* n_preallocs */
			(GInstanceInitFunc) mime_types_model_init,
			NULL
		};

		static const GInterfaceInfo tree_model_info = {
			(GInterfaceInitFunc) mime_types_model_tree_model_init,
			NULL,
			NULL
		};

		mime_types_model_type = 
			g_type_register_static (G_TYPE_OBJECT, 
						"MimeTypesModel",
						&mime_types_model_info, 0);

		g_type_add_interface_static (mime_types_model_type,
					     GTK_TYPE_TREE_MODEL,
					     &tree_model_info);
	}

	return mime_types_model_type;
}

static void
mime_types_model_init (MimeTypesModel *mime_types_model, MimeTypesModelClass *class)
{
	mime_types_model->p = g_new0 (MimeTypesModelPrivate, 1);
	mime_types_model->p->stamp = g_random_int ();
}

static void
mime_types_model_base_init (MimeTypesModelClass *class) 
{
}

static void
mime_types_model_class_init (MimeTypesModelClass *class) 
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	object_class->finalize = mime_types_model_finalize;
	object_class->set_property = mime_types_model_set_prop;
	object_class->get_property = mime_types_model_get_prop;

	g_object_class_install_property
		(object_class, PROP_CATEGORY_ONLY,
		 g_param_spec_boolean ("category-only",
				       _("Model for categories only"),
				       _("Model for categories only"),
				       FALSE,
				       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	parent_class = G_OBJECT_CLASS
		(g_type_class_ref (G_TYPE_OBJECT));
}

static void
mime_types_model_tree_model_init (GtkTreeModelIface *iface)
{
	iface->get_flags       = mime_types_model_get_flags;
	iface->get_n_columns   = mime_types_model_get_n_columns;
	iface->get_column_type = mime_types_model_get_column_type;
	iface->get_iter        = mime_types_model_get_iter;
	iface->get_path        = mime_types_model_get_path;
	iface->get_value       = mime_types_model_get_value;
	iface->iter_next       = mime_types_model_iter_next;
	iface->iter_children   = mime_types_model_iter_children;
	iface->iter_has_child  = mime_types_model_iter_has_child;
	iface->iter_n_children = mime_types_model_iter_n_children;
	iface->iter_nth_child  = mime_types_model_iter_nth_child;
	iface->iter_parent     = mime_types_model_iter_parent;
}

static void
mime_types_model_set_prop (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) 
{
	MimeTypesModel *mime_types_model;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_MIME_TYPES_MODEL (object));

	mime_types_model = MIME_TYPES_MODEL (object);

	switch (prop_id) {
	case PROP_CATEGORY_ONLY:
		mime_types_model->p->category_only = g_value_get_boolean (value);
		break;

	default:
		g_warning ("Bad property set");
		break;
	}
}

static void
mime_types_model_get_prop (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) 
{
	MimeTypesModel *mime_types_model;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_MIME_TYPES_MODEL (object));

	mime_types_model = MIME_TYPES_MODEL (object);

	switch (prop_id) {
	case PROP_CATEGORY_ONLY:
		g_value_set_boolean (value, mime_types_model->p->category_only);
		break;

	default:
		g_warning ("Bad property get");
		break;
	}
}

static void
mime_types_model_finalize (GObject *object) 
{
	MimeTypesModel *mime_types_model;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_MIME_TYPES_MODEL (object));

	mime_types_model = MIME_TYPES_MODEL (object);

	g_free (mime_types_model->p);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}



GObject *
mime_types_model_new (gboolean category_only) 
{
	return g_object_new (mime_types_model_get_type (),
			     "category-only", category_only,
			     NULL);
}

void
mime_types_model_construct_iter (MimeTypesModel *model, ModelEntry *entry, GtkTreeIter *iter)
{
	g_return_if_fail (model != NULL);
	g_return_if_fail (IS_MIME_TYPES_MODEL (model));
	g_return_if_fail (iter != NULL);

	iter->stamp = model->p->stamp;
	iter->user_data = entry;
}



static guint
mime_types_model_get_flags (GtkTreeModel *tree_model)
{
	g_return_val_if_fail (tree_model != NULL, 0);
	g_return_val_if_fail (IS_MIME_TYPES_MODEL (tree_model), 0);

	return GTK_TREE_MODEL_ITERS_PERSIST;
}

static gint
mime_types_model_get_n_columns (GtkTreeModel *tree_model)
{
	g_return_val_if_fail (tree_model != NULL, 0);
	g_return_val_if_fail (IS_MIME_TYPES_MODEL (tree_model), 0);

	return MODEL_LAST_COLUMN;
}

static GType
mime_types_model_get_column_type (GtkTreeModel *tree_model, gint index)
{
	g_return_val_if_fail (tree_model != NULL, G_TYPE_INVALID);
	g_return_val_if_fail (IS_MIME_TYPES_MODEL (tree_model), G_TYPE_INVALID);

	switch (index) {
	case MODEL_COLUMN_MIME_TYPE:
	case MODEL_COLUMN_DESCRIPTION:
	case MODEL_COLUMN_FILE_EXT:
		return G_TYPE_STRING;
		break;

	case MODEL_COLUMN_ICON:
		return GDK_TYPE_PIXBUF;
		break;

	default:
		return G_TYPE_INVALID;
		break;
	}
}

/* Adapted from gtk+/gtk/gtktreestore.c, gtk_tree_store_get_iter */

static gboolean
mime_types_model_get_iter (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreePath *path)
{
	MimeTypesModel *model;
	gint *indices, depth, i;
	GtkTreeIter parent;

	g_return_val_if_fail (tree_model != NULL, FALSE);
	g_return_val_if_fail (IS_MIME_TYPES_MODEL (tree_model), FALSE);

	model = MIME_TYPES_MODEL (tree_model);

	indices = gtk_tree_path_get_indices (path);
	depth = gtk_tree_path_get_depth (path);

	g_return_val_if_fail (depth > 0, FALSE);

	parent.stamp = model->p->stamp;
	parent.user_data = get_model_entries ();

	if (!gtk_tree_model_iter_nth_child (tree_model, iter, &parent, indices[0]))
		return FALSE;

	for (i = 1; i < depth; i++) {
		parent = *iter;
		if (!gtk_tree_model_iter_nth_child (tree_model, iter, &parent, indices[i]))
			return FALSE;
	}

	return TRUE;
}

static GtkTreePath *
mime_types_model_get_path (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	MimeTypesModel *model;
	ModelEntry *entry;
	GtkTreePath *path;

	g_return_val_if_fail (tree_model != NULL, NULL);
	g_return_val_if_fail (IS_MIME_TYPES_MODEL (tree_model), NULL);

	model = MIME_TYPES_MODEL (tree_model);

	path = gtk_tree_path_new ();
	entry = iter->user_data;

	while (entry->parent != NULL) {
		gtk_tree_path_prepend_index (path, model_entry_get_index (entry->parent, entry));
		entry = entry->parent;
	}

	return path;
}

static void
mime_types_model_get_value (GtkTreeModel *tree_model, GtkTreeIter *iter, gint column, GValue *value)
{
	MimeTypesModel *model;
	ModelEntry *entry;

	g_return_if_fail (tree_model != NULL);
	g_return_if_fail (IS_MIME_TYPES_MODEL (tree_model));

	model = MIME_TYPES_MODEL (tree_model);
	entry = iter->user_data;

	switch (column) {
	case MODEL_COLUMN_MIME_TYPE:
		g_value_init (value, G_TYPE_STRING);

		switch (entry->type) {
		case MODEL_ENTRY_MIME_TYPE:
			g_value_set_static_string (value, MIME_TYPE_INFO (entry)->mime_type);
			break;

		case MODEL_ENTRY_SERVICE:
			g_value_set_static_string (value, SERVICE_INFO (entry)->protocol);
			break;

		default:
			g_value_set_string (value, NULL);
			break;
		}

		break;

	case MODEL_COLUMN_DESCRIPTION:
		g_value_init (value, G_TYPE_STRING);

		switch (entry->type) {
		case MODEL_ENTRY_MIME_TYPE:
			g_value_set_static_string (value, mime_type_info_get_description (MIME_TYPE_INFO (entry)));
			break;

		case MODEL_ENTRY_CATEGORY:
			g_value_set_static_string (value, MIME_CATEGORY_INFO (entry)->name);
			break;

		case MODEL_ENTRY_SERVICE:
			g_value_set_static_string (value, service_info_get_description (SERVICE_INFO (entry)));
			break;

		case MODEL_ENTRY_SERVICES_CATEGORY:
			g_value_set_static_string (value, _("Internet Services"));
			break;

		default:
			g_value_set_string (value, NULL);
			break;
		}

		break;

	case MODEL_COLUMN_ICON:
		g_value_init (value, GDK_TYPE_PIXBUF);

		switch (entry->type) {
		case MODEL_ENTRY_MIME_TYPE:
			g_value_set_object (value, G_OBJECT (mime_type_info_get_icon (MIME_TYPE_INFO (entry))));
			break;

		default:
			g_value_set_object (value, NULL);
			break;
		}

		break;

	case MODEL_COLUMN_FILE_EXT:
		g_value_init (value, G_TYPE_STRING);

		switch (entry->type) {
		case MODEL_ENTRY_MIME_TYPE:
			g_value_set_string (value, mime_type_info_get_file_extensions_pretty_string (MIME_TYPE_INFO (entry)));
			break;

		case MODEL_ENTRY_SERVICE:
			g_value_set_static_string (value, SERVICE_INFO (entry)->protocol);
			break;

		default:
			g_value_set_string (value, NULL);
			break;
		}

		break;
	}
}

static gboolean
mime_types_model_iter_next (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	MimeTypesModel *model;
	ModelEntry *entry;

	g_return_val_if_fail (tree_model != NULL, FALSE);
	g_return_val_if_fail (IS_MIME_TYPES_MODEL (tree_model), FALSE);

	model = MIME_TYPES_MODEL (tree_model);
	entry = iter->user_data;

	if (entry != NULL)
		iter->user_data = entry->next;

	if (model->p->category_only)
		while (iter->user_data != NULL && !IS_CATEGORY (MODEL_ENTRY (iter->user_data)))
			iter->user_data = MODEL_ENTRY (iter->user_data)->next;

	return iter->user_data != NULL;
}

static gboolean
mime_types_model_iter_children (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent)
{
	MimeTypesModel *model;
	ModelEntry *entry;

	g_return_val_if_fail (tree_model != NULL, FALSE);
	g_return_val_if_fail (IS_MIME_TYPES_MODEL (tree_model), FALSE);

	model = MIME_TYPES_MODEL (tree_model);

	if (parent != NULL)
		entry = parent->user_data;
	else
		entry = NULL;

	if (entry == NULL)
		iter->user_data = get_model_entries ();
	else
		iter->user_data = entry->first_child;

	if (model->p->category_only)
		while (iter->user_data != NULL && !IS_CATEGORY (MODEL_ENTRY (iter->user_data)))
			iter->user_data = MODEL_ENTRY (iter->user_data)->next;

	return iter->user_data != NULL;
}

static gboolean
mime_types_model_iter_has_child (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	MimeTypesModel *model;
	ModelEntry *entry;

	g_return_val_if_fail (tree_model != NULL, FALSE);
	g_return_val_if_fail (IS_MIME_TYPES_MODEL (tree_model), FALSE);

	model = MIME_TYPES_MODEL (tree_model);
	entry = iter->user_data;

	if (entry == NULL)
		return get_model_entries ()->first_child != NULL;
	else if (!model->p->category_only || IS_CATEGORY (entry->first_child))
		return entry->first_child != NULL;
	else {
		for (entry = entry->first_child; entry != NULL; entry = entry->next)
			if (IS_CATEGORY (entry))
				return TRUE;

		return FALSE;
	}
}

static gint
mime_types_model_iter_n_children (GtkTreeModel *tree_model, GtkTreeIter *iter)
{
	MimeTypesModel *model;
	ModelEntry *entry, *tmp;
	gint count = 0;

	g_return_val_if_fail (tree_model != NULL, 0);
	g_return_val_if_fail (IS_MIME_TYPES_MODEL (tree_model), 0);

	model = MIME_TYPES_MODEL (tree_model);

	if (iter != NULL)
		entry = iter->user_data;
	else
		entry = NULL;

	if (entry == NULL)
		entry = get_model_entries ();

	if (model->p->category_only) {
		for (tmp = entry->first_child; tmp != NULL; tmp = tmp->next) {
			if (tmp->type == MODEL_ENTRY_CATEGORY)
				count++;
		}
	} else {
		for (tmp = entry->first_child; tmp != NULL; tmp = tmp->next)
			count++;
	}

	return count;
}

static gboolean
mime_types_model_iter_nth_child (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *parent, gint n)
{
	MimeTypesModel *model;
	ModelEntry *entry;

	g_return_val_if_fail (tree_model != NULL, FALSE);
	g_return_val_if_fail (IS_MIME_TYPES_MODEL (tree_model), FALSE);

	model = MIME_TYPES_MODEL (tree_model);

	if (parent != NULL)
		entry = parent->user_data;
	else
		entry = NULL;

	if (entry == NULL)
		iter->user_data = model_entry_get_nth_child (get_model_entries (), n, model->p->category_only);
	else
		iter->user_data = model_entry_get_nth_child (entry, n, model->p->category_only);

	return iter->user_data != NULL;
}

static gboolean
mime_types_model_iter_parent (GtkTreeModel *tree_model, GtkTreeIter *iter, GtkTreeIter *child)
{
	MimeTypesModel *model;
	ModelEntry *entry;

	g_return_val_if_fail (tree_model != NULL, FALSE);
	g_return_val_if_fail (IS_MIME_TYPES_MODEL (tree_model), FALSE);

	model = MIME_TYPES_MODEL (tree_model);
	entry = iter->user_data;

	if (entry != NULL)
		iter->user_data = entry->parent;

	return iter->user_data != NULL;
}

