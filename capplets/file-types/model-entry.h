/* -*- mode: c; style: linux -*- */

/* model-entry.h
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

#ifndef __MODEL_ENTRY_H
#define __MODEL_ENTRY_H

#include <gnome.h>

G_BEGIN_DECLS

typedef struct _ModelEntry ModelEntry;

typedef enum {
	MODEL_ENTRY_NONE, MODEL_ENTRY_CATEGORY, MODEL_ENTRY_SERVICES_CATEGORY,
	MODEL_ENTRY_MIME_TYPE, MODEL_ENTRY_SERVICE
} ModelEntryType;

#define MODEL_ENTRY(obj) ((ModelEntry *) obj)

struct _ModelEntry
{
	ModelEntryType      type;

	struct _ModelEntry *next;
	struct _ModelEntry *parent;
	struct _ModelEntry *first_child;
};

ModelEntry *get_model_entries         (void);

ModelEntry *model_entry_get_nth_child (ModelEntry *entry,
			               gint        n,
				       gboolean    categories_only);

gint        model_entry_get_index     (ModelEntry *parent,
				       ModelEntry *child);

void        model_entry_insert_child  (ModelEntry *entry,
				       ModelEntry *child);
void        model_entry_remove_child  (ModelEntry *entry,
				       ModelEntry *child);

G_END_DECLS

#endif /* __MODEL_ENTRY_H */
