/* -*- mode: c; style: linux -*- */

/* rollback-widget.h
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

#ifndef __ROLLBACK_WIDGET_H
#define __ROLLBACK_WIDGET_H

#include <gnome.h>

BEGIN_GNOME_DECLS

#define ROLLBACK_WIDGET(obj)          GTK_CHECK_CAST (obj, rollback_widget_get_type (), RollbackWidget)
#define ROLLBACK_WIDGET_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, rollback_widget_get_type (), RollbackWidgetClass)
#define IS_ROLLBACK_WIDGET(obj)       GTK_CHECK_TYPE (obj, rollback_widget_get_type ())

typedef struct _RollbackWidget RollbackWidget;
typedef struct _RollbackWidgetClass RollbackWidgetClass;
typedef struct _RollbackWidgetPrivate RollbackWidgetPrivate;

struct _RollbackWidget 
{
	GtkWidget parent;

	RollbackWidgetPrivate *p;
};

struct _RollbackWidgetClass 
{
	GtkWidgetClass gtk_widget_class;
};

guint rollback_widget_get_type         (void);

GtkObject *rollback_widget_new         (void);

END_GNOME_DECLS

#endif /* __ROLLBACK_WIDGET_H */
