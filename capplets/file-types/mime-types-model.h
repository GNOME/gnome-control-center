/* -*- mode: c; style: linux -*- */

/* mime-types-model.h
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

#ifndef __MIME_TYPES_MODEL_H
#define __MIME_TYPES_MODEL_H

#include <gnome.h>

G_BEGIN_DECLS

enum {
	ICON_COLUMN,
	DESCRIPTION_COLUMN,
	MIME_TYPE_COLUMN,
	EXTENSIONS_COLUMN
};

GtkTreeModel *mime_types_model_new         (gboolean      is_category_select);

void          reinsert_model_entry         (GtkTreeModel *model,
					    GtkTreeIter  *iter);

GdkPixbuf    *get_icon_pixbuf              (const gchar  *short_icon_name);
gchar        *get_description_for_protocol (const gchar  *protocol_name);

gchar        *get_category_name            (GtkTreeModel *model,
					    GtkTreeIter  *iter,
					    gboolean      include_iter);
void          get_insertion_point          (GtkTreeStore *model,
					    const gchar  *path_str,
					    GtkTreeIter  *iter);

gboolean      model_entry_is_protocol      (GtkTreeModel *model,
					    GtkTreeIter  *iter);
gboolean      model_entry_is_category      (GtkTreeModel *model,
					    GtkTreeIter  *iter);
gboolean      model_entry_is_internet_services_category (GtkTreeModel *model,
							 GtkTreeIter *iter);

GList        *find_supported_apps_for_category     (GtkTreeModel      *model,
						    GtkTreeIter       *iter);

G_END_DECLS

#endif /* __MIME_TYPES_MODEL_H */
