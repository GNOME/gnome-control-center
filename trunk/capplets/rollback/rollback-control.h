/* -*- mode: c; style: linux -*- */

/* rollback-control.h
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

#ifndef __ROLLBACK_CONTROL_H
#define __ROLLBACK_CONTROL_H

#include <gnome.h>


BEGIN_GNOME_DECLS

#define ROLLBACK_CONTROL(obj)          GTK_CHECK_CAST (obj, rollback_control_get_type (), RollbackControl)
#define ROLLBACK_CONTROL_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, rollback_control_get_type (), RollbackControlClass)
#define IS_ROLLBACK_CONTROL(obj)       GTK_CHECK_TYPE (obj, rollback_control_get_type ())

typedef struct _RollbackControl RollbackControl;
typedef struct _RollbackControlClass RollbackControlClass;
typedef struct _RollbackControlPrivate RollbackControlPrivate;

struct _RollbackControl 
{
	GnomeCanvasItem parent;

	RollbackControlPrivate *p;
};

struct _RollbackControlClass 
{
	GnomeCanvasItemClass gnome_canvas_item_class;
};

GType rollback_control_get_type         (void);

END_GNOME_DECLS

#endif /* __ROLLBACK_CONTROL_H */
