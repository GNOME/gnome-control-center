/* -*- mode: c; style: linux -*- */

/* rollback-widget.c
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "rollback-widget.h"

enum {
	ARG_0,
	ARG_SAMPLE
};

struct _RollbackWidgetPrivate 
{
	GdkGC    *main_gc;
};

static GnomeCanvasClass *parent_class;

static void rollback_widget_init        (RollbackWidget *rollback_widget);
static void rollback_widget_class_init  (RollbackWidgetClass *class);

static void rollback_widget_set_arg     (GtkObject *object, 
					 GtkArg *arg, 
					 guint arg_id);
static void rollback_widget_get_arg     (GtkObject *object, 
					 GtkArg *arg, 
					 guint arg_id);

static void rollback_widget_finalize    (GtkObject *object);

static void rollback_widget_realize     (GtkWidget *widget);
static void rollback_widget_unrealize   (GtkWidget *widget);

GType
rollback_widget_get_type (void)
{
	static GType rollback_widget_type = 0;

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
			gtk_type_unique (gnome_canvas_get_type (), 
					 &rollback_widget_info);
	}

	return rollback_widget_type;
}

static void
rollback_widget_init (RollbackWidget *rollback_widget)
{
	rollback_widget->p = g_new0 (RollbackWidgetPrivate, 1);
	gtk_widget_set_usize (GTK_WIDGET (rollback_widget), 200, 30);

	rollback_widget->control_colors[BACKGROUND_COLOR].red = 112 * 256;
	rollback_widget->control_colors[BACKGROUND_COLOR].green = 128 * 256;
	rollback_widget->control_colors[BACKGROUND_COLOR].blue = 144 * 256;

	rollback_widget->control_colors[MARKER_COLOR].red = 0;
	rollback_widget->control_colors[MARKER_COLOR].green = 0;
	rollback_widget->control_colors[MARKER_COLOR].blue = 0;
}

static void
rollback_widget_class_init (RollbackWidgetClass *class) 
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	gtk_object_add_arg_type ("RollbackWidget::sample",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_SAMPLE);

	object_class = GTK_OBJECT_CLASS (class);
	object_class->finalize = rollback_widget_finalize;
	object_class->set_arg = rollback_widget_set_arg;
	object_class->get_arg = rollback_widget_get_arg;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = rollback_widget_realize;
	widget_class->unrealize = rollback_widget_unrealize;

	parent_class = GNOME_CANVAS_CLASS
		(gtk_type_class (gnome_canvas_get_type ()));
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

GdkGC *
rollback_widget_get_gc (RollbackWidget *widget)
{
	g_return_val_if_fail (widget != NULL, NULL);
	g_return_val_if_fail (IS_ROLLBACK_WIDGET (widget), NULL);

	gdk_gc_ref (widget->p->main_gc);
	return widget->p->main_gc;
}

static void
rollback_widget_realize (GtkWidget *widget)
{
	RollbackWidget *rollback_widget;
	GdkColormap *colormap;
	gboolean success[LAST_COLOR];
	gint i;

	rollback_widget = ROLLBACK_WIDGET (widget);

	if (!GTK_WIDGET_REALIZED (widget)) {
		GTK_WIDGET_CLASS (parent_class)->realize (widget);
		rollback_widget->p->main_gc = gdk_gc_new (widget->window);

		colormap = gtk_widget_get_colormap (widget);
		gdk_colormap_alloc_colors (colormap,
					   rollback_widget->control_colors,
					   LAST_COLOR, FALSE, TRUE,
					   success);

		for (i = 0; success[i] && i < LAST_COLOR; i++);

		if (i < LAST_COLOR)
			g_warning ("Could not allocate colors for rollback "
				   "control\n");
	}
}

static void
rollback_widget_unrealize (GtkWidget *widget) 
{
	if (ROLLBACK_WIDGET (widget)->p->main_gc != NULL) {
		gdk_gc_unref (ROLLBACK_WIDGET (widget)->p->main_gc);
		ROLLBACK_WIDGET (widget)->p->main_gc = NULL;
	}

	GTK_WIDGET_CLASS (parent_class)->unrealize (widget);
}
