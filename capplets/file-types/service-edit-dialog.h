/* -*- mode: c; style: linux -*- */

/* service-edit-dialog.h
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

#ifndef __SERVICE_EDIT_DIALOG_H
#define __SERVICE_EDIT_DIALOG_H

#include <gnome.h>

#include "service-info.h"

G_BEGIN_DECLS

#define SERVICE_EDIT_DIALOG(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, service_edit_dialog_get_type (), ServiceEditDialog)
#define SERVICE_EDIT_DIALOG_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, service_edit_dialog_get_type (), ServiceEditDialogClass)
#define IS_SERVICE_EDIT_DIALOG(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, service_edit_dialog_get_type ())

typedef struct _ServiceEditDialog ServiceEditDialog;
typedef struct _ServiceEditDialogClass ServiceEditDialogClass;
typedef struct _ServiceEditDialogPrivate ServiceEditDialogPrivate;

struct _ServiceEditDialog 
{
	GObject parent;

	ServiceEditDialogPrivate *p;
};

struct _ServiceEditDialogClass 
{
	GObjectClass g_object_class;
};

GType service_edit_dialog_get_type (void);

GObject *service_edit_dialog_new   (GtkTreeModel *model,
				    GtkTreeIter  *iter);

G_END_DECLS

#endif /* __SERVICE_EDIT_DIALOG_H */
