/* -*- mode: c; style: linux -*- */

/* mime-types-model.h
 * Copyright (C) 2001 Ximian, Inc.
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

#ifndef __MIME_TYPES_MODEL_H
#define __MIME_TYPES_MODEL_H

#include <gnome.h>

#include "model-entry.h"

G_BEGIN_DECLS

#define MIME_TYPES_MODEL(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, mime_types_model_get_type (), MimeTypesModel)
#define MIME_TYPES_MODEL_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, mime_types_model_get_type (), MimeTypesModelClass)
#define IS_MIME_TYPES_MODEL(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, mime_types_model_get_type ())

#define MODEL_ENTRY_FROM_ITER(obj) (MODEL_ENTRY ((obj)->user_data))

typedef struct _MimeTypesModel MimeTypesModel;
typedef struct _MimeTypesModelClass MimeTypesModelClass;
typedef struct _MimeTypesModelPrivate MimeTypesModelPrivate;

enum {
	MODEL_COLUMN_MIME_TYPE,
	MODEL_COLUMN_DESCRIPTION,
	MODEL_COLUMN_ICON,
	MODEL_COLUMN_FILE_EXT,
	MODEL_LAST_COLUMN
};

struct _MimeTypesModel 
{
	GObject parent;

	MimeTypesModelPrivate *p;
};

struct _MimeTypesModelClass 
{
	GObjectClass g_object_class;
};

GType    mime_types_model_get_type       (void);

GObject *mime_types_model_new            (gboolean category_only);

void     mime_types_model_construct_iter (MimeTypesModel *model,
					  ModelEntry     *entry,
					  GtkTreeIter    *iter);

G_END_DECLS

#endif /* __MIME_TYPES_MODEL_H */
