/* -*- mode: c; style: linux -*- */

/* create-location-dialog.h
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

#ifndef __CREATE_LOCATION_DIALOG_H
#define __CREATE_LOCATION_DIALOG_H

#include <gnome.h>

#include "config-manager-dialog.h"
#include "location.h"

BEGIN_GNOME_DECLS

#define CREATE_LOCATION_DIALOG(obj)          GTK_CHECK_CAST (obj, create_location_dialog_get_type (), CreateLocationDialog)
#define CREATE_LOCATION_DIALOG_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, create_location_dialog_get_type (), CreateLocationDialogClass)
#define IS_CREATE_LOCATION_DIALOG(obj)       GTK_CHECK_TYPE (obj, create_location_dialog_get_type ())

typedef struct _CreateLocationDialog CreateLocationDialog;
typedef struct _CreateLocationDialogClass CreateLocationDialogClass;
typedef struct _CreateLocationDialogPrivate CreateLocationDialogPrivate;

struct _CreateLocationDialog 
{
	GnomeDialog parent;

	CreateLocationDialogPrivate *p;
};

struct _CreateLocationDialogClass 
{
	GnomeDialogClass gnome_dialog_class;

	void (*create_location) (CreateLocationDialog *, gchar *, Location *);
};

guint create_location_dialog_get_type         (void);

GtkObject *create_location_dialog_new         (CMDialogType type);

END_GNOME_DECLS

#endif /* __CREATE_LOCATION_DIALOG_H */
