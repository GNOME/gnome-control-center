/* -*- mode: c; style: linux -*- */

/* mime-edit-dialog.h
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

#ifndef __MIME_EDIT_DIALOG_H
#define __MIME_EDIT_DIALOG_H

#include <gnome.h>

#include "mime-type-info.h"

G_BEGIN_DECLS

#define MIME_EDIT_DIALOG(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, mime_edit_dialog_get_type (), MimeEditDialog)
#define MIME_EDIT_DIALOG_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, mime_edit_dialog_get_type (), MimeEditDialogClass)
#define IS_MIME_EDIT_DIALOG(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, mime_edit_dialog_get_type ())

typedef struct _MimeEditDialog MimeEditDialog;
typedef struct _MimeEditDialogClass MimeEditDialogClass;
typedef struct _MimeEditDialogPrivate MimeEditDialogPrivate;

struct _MimeEditDialog 
{
	GObject parent;

	MimeEditDialogPrivate *p;
};

struct _MimeEditDialogClass 
{
	GObjectClass g_object_class;

	void (*done) (MimeEditDialog *dialog, gboolean ok);
};

GType mime_edit_dialog_get_type         (void);

GObject *mime_edit_dialog_new           (GtkTreeModel *model,
					 MimeTypeInfo *info);
GObject *mime_add_dialog_new            (GtkTreeModel *model, GtkWindow *parent,
					 char const *file_name);

void mime_edit_dialog_get_app (GladeXML *glade, char const *mime_type,
			       GnomeVFSMimeApplication **current);

G_END_DECLS

#endif /* __MIME_EDIT_DIALOG_H */
