/* -*- mode: c; style: linux -*- */

/* backend-list.h
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

#ifndef __BACKEND_LIST_H
#define __BACKEND_LIST_H

#include <gnome.h>
#include <bonobo.h>

#include "ConfigArchiver.h"

BEGIN_GNOME_DECLS

#define BACKEND_LIST(obj)          GTK_CHECK_CAST (obj, backend_list_get_type (), BackendList)
#define BACKEND_LIST_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, backend_list_get_type (), BackendListClass)
#define IS_BACKEND_LIST(obj)       GTK_CHECK_TYPE (obj, backend_list_get_type ())

typedef struct _BackendList BackendList;
typedef struct _BackendListClass BackendListClass;
typedef struct _BackendListPrivate BackendListPrivate;

typedef gint (*BackendCB) (BackendList *, gchar *, gpointer);

struct _BackendList 
{
	BonoboXObject parent;

	BackendListPrivate *p;
};

struct _BackendListClass 
{
	BonoboXObjectClass parent_class;

	POA_ConfigArchiver_BackendList__epv epv;
};

guint         backend_list_get_type    (void);

BonoboObject *backend_list_new         (gboolean is_global);

gboolean      backend_list_contains    (BackendList *backend_list,
					const gchar *backend_id);

gboolean      backend_list_foreach     (BackendList *backend_list,
					BackendCB callback,
					gpointer data);

void          backend_list_add         (BackendList *backend_list,
					const gchar *backend_id);
void          backend_list_remove      (BackendList *backend_list,
					const gchar *backend_id);

void          backend_list_save        (BackendList *backend_list);

END_GNOME_DECLS

#endif /* __BACKEND_LIST_H */
