/* -*- mode: c; style: linux -*- */

/* rollback-widget.c
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_LIBARCHIVER

#include "rollback-widget.h"

enum {
	ARG_0,
	ARG_SAMPLE
};

struct _RollbackWidgetPrivate 
{
	/* Private data members */
};

static GtkWidgetClass *parent_class;

static void rollback_widget_init        (RollbackWidget *rollback_widget);
static void rollback_widget_class_init  (RollbackWidgetClass *class);

static void rollback_widget_set_arg     (GtkObject *object, 
					   GtkArg *arg, 
					   guint arg_id);
static void rollback_widget_get_arg     (GtkObject *object, 
					   GtkArg *arg, 
					   guint arg_id);

static void rollback_widget_finalize    (GtkObject *object);

guint
rollback_widget_get_type (void)
{
	static guint rollback_widget_type = 0;

	if (!rollback_widget_type) {
		GtkTypeInfo rollback_widget_info = {
			"RollbackWidget",
			sizeof (RollbackWidget),
			sizeof (RollbackWidgetClass),
			(GtkClassInitFunc) rollback_widget_class_init,
			(GtkObjectInitFunc) rollback_widget_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		rollback_widget_type = 
			gtk_type_unique (gtk_widget_get_type (), 
					 &rollback_widget_info);
	}

	return rollback_widget_type;
}

static void
rollback_widget_init (RollbackWidget *rollback_widget)
{
	rollback_widget->p = g_new0 (RollbackWidgetPrivate, 1);
}

static void
rollback_widget_class_init (RollbackWidgetClass *class) 
{
	GtkObjectClass *object_class;

	gtk_object_add_arg_type ("RollbackWidget::sample",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_SAMPLE);

	object_class = GTK_OBJECT_CLASS (class);
	object_class->finalize = rollback_widget_finalize;
	object_class->set_arg = rollback_widget_set_arg;
	object_class->get_arg = rollback_widget_get_arg;

	parent_class = GTK_WIDGET_CLASS
		(gtk_type_class (gtk_widget_get_type ()));
}

static void
rollback_widget_set_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	RollbackWidget *rollback_widget;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ROLLBACK_WIDGET (object));

	rollback_widget = ROLLBACK_WIDGET (object);

	switch (arg_id) {
	case ARG_SAMPLE:
		break;

	default:
		g_warning ("Bad argument set");
		break;
	}
}

static void
rollback_widget_get_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	RollbackWidget *rollback_widget;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ROLLBACK_WIDGET (object));

	rollback_widget = ROLLBACK_WIDGET (object);

	switch (arg_id) {
	case ARG_SAMPLE:
		break;

	default:
		g_warning ("Bad argument get");
		break;
	}
}

static void
rollback_widget_finalize (GtkObject *object) 
{
	RollbackWidget *rollback_widget;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ROLLBACK_WIDGET (object));

	rollback_widget = ROLLBACK_WIDGET (object);

	g_free (rollback_widget->p);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

GtkObject *
rollback_widget_new (void) 
{
	return gtk_object_new (rollback_widget_get_type (),
			       NULL);
}

#endif HAVE_LIBARCHIVER
