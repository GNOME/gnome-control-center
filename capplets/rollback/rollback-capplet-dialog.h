/* -*- mode: c; style: linux -*- */

/* rollback-capplet-dialog.h
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

#ifndef __ROLLBACK_CAPPLET_DIALOG_H
#define __ROLLBACK_CAPPLET_DIALOG_H

#include <gnome.h>


BEGIN_GNOME_DECLS

#define ROLLBACK_CAPPLET_DIALOG(obj)          GTK_CHECK_CAST (obj, rollback_capplet_dialog_get_type (), RollbackCappletDialog)
#define ROLLBACK_CAPPLET_DIALOG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, rollback_capplet_dialog_get_type (), RollbackCappletDialogClass)
#define IS_ROLLBACK_CAPPLET_DIALOG(obj)       GTK_CHECK_TYPE (obj, rollback_capplet_dialog_get_type ())

typedef struct _RollbackCappletDialog RollbackCappletDialog;
typedef struct _RollbackCappletDialogClass RollbackCappletDialogClass;
typedef struct _RollbackCappletDialogPrivate RollbackCappletDialogPrivate;

struct _RollbackCappletDialog 
{
	GnomeDialog parent;

	RollbackCappletDialogPrivate *p;
};

struct _RollbackCappletDialogClass 
{
	GnomeDialogClass gnome_dialog_class;
};

guint rollback_capplet_dialog_get_type         (void);

GtkObject *rollback_capplet_dialog_new         (gchar *capplet_name);

END_GNOME_DECLS

#endif /* __ROLLBACK_CAPPLET_DIALOG_H */
