/* -*- mode: c; style: linux -*- */

/* location-list.h
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen <hovinen@helixcode.com>
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

#ifndef __LOCATION_LIST_H
#define __LOCATION_LIST_H

#include <gnome.h>

#include "archive.h"
#include "location.h"
#include "config-manager-dialog.h"

BEGIN_GNOME_DECLS

#define LOCATION_LIST(obj)          GTK_CHECK_CAST (obj, location_list_get_type (), LocationList)
#define LOCATION_LIST_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, location_list_get_type (), LocationListClass)
#define IS_LOCATION_LIST(obj)       GTK_CHECK_TYPE (obj, location_list_get_type ())

typedef struct _LocationList LocationList;
typedef struct _LocationListClass LocationListClass;
typedef struct _LocationListPrivate LocationListPrivate;

struct _LocationList 
{
	GtkCTree parent;

	LocationListPrivate *p;
};

struct _LocationListClass 
{
	GtkCTreeClass gtk_ctree_class;
};

guint      location_list_get_type                  (void);

GtkWidget *location_list_new                       (gboolean sep_locations,
						    Archive *user_archive,
						    Archive *global_archive);

gchar     *location_list_get_selected_location_id  (LocationList *list);
Location  *location_list_get_selected_location     (LocationList *list);

void       location_list_reread                    (LocationList *list);

END_GNOME_DECLS

#endif /* __LOCATION_LIST_H */
