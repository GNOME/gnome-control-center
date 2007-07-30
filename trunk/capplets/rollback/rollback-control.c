/* -*- mode: c; style: linux -*- */

/* rollback-control.c
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

#include <archive.h>
#include <location.h>
#include <config-log.h>

#include "rollback-control.h"
#include "rollback-widget.h"

#define CONTROL_HEIGHT 30
#define MARKER_SIZE 8

enum {
	ARG_0,
	ARG_CONTROL_NUMBER,
	ARG_BACKEND_ID,
	ARG_IS_GLOBAL
};

struct _RollbackControlPrivate 
{
	Archive       *archive;
	Location      *location;
	ConfigLog     *config_log;
	gchar         *backend_id;
	gint           y;             /* y-coordonite of the canvas item */

	GdkDrawable   *drawable;
};

static GnomeCanvasItemClass *parent_class;

static void rollback_control_init        (RollbackControl *rollback_control);
static void rollback_control_class_init  (RollbackControlClass *class);

static void rollback_control_set_arg     (GtkObject *object, 
					  GtkArg *arg, 
					  guint arg_id);
static void rollback_control_get_arg     (GtkObject *object, 
					  GtkArg *arg, 
					  guint arg_id);

static void rollback_control_finalize    (GtkObject *object);

static void rollback_control_update      (GnomeCanvasItem *item,
					  double affine[6],
					  ArtSVP *clip_path,
					  gint flags);
static void rollback_control_draw        (GnomeCanvasItem *item,
					  GdkDrawable *drawable,
					  int x, int y,
					  int width, int height);

static int draw_markers_cb               (ConfigLog *config_log,
					  gint id,
					  gchar *backend_id,
					  struct tm *time,
					  RollbackControl *control);
static gint time_to_x                    (time_t t, gint width);
static gint horner                       (gint *coeff, gint degree, 
					  gint divisor, gint x);

GType
rollback_control_get_type (void)
{
	static GType rollback_control_type = 0;
	
	if (!rollback_control_type) {
		GtkTypeInfo rollback_control_info = {
			"RollbackControl",
			sizeof (RollbackControl),
			sizeof (RollbackControlClass),
			(GtkClassInitFunc) rollback_control_class_init,
			(GtkObjectInitFunc) rollback_control_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		rollback_control_type = 
			gtk_type_unique (gnome_canvas_item_get_type (), 
					 &rollback_control_info);
	}

	return rollback_control_type;
}

static void
rollback_control_init (RollbackControl *rollback_control)
{
	rollback_control->p = g_new0 (RollbackControlPrivate, 1);
}

static void
rollback_control_class_init (RollbackControlClass *class) 
{
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *canvas_item_class;

	gtk_object_add_arg_type ("RollbackControl::control-number",
				 GTK_TYPE_INT,
				 GTK_ARG_READWRITE,
				 ARG_CONTROL_NUMBER);
	gtk_object_add_arg_type ("RollbackControl::backend-id",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT_ONLY,
				 ARG_BACKEND_ID);
	gtk_object_add_arg_type ("RollbackControl::is-global",
				 GTK_TYPE_INT,
				 GTK_ARG_READWRITE | GTK_ARG_CONSTRUCT_ONLY,
				 ARG_IS_GLOBAL);

	object_class = GTK_OBJECT_CLASS (class);
	object_class->finalize = rollback_control_finalize;
	object_class->set_arg = rollback_control_set_arg;
	object_class->get_arg = rollback_control_get_arg;

	canvas_item_class = GNOME_CANVAS_ITEM_CLASS (class);
	canvas_item_class->update = rollback_control_update;
	canvas_item_class->draw = rollback_control_draw;

	parent_class = GNOME_CANVAS_ITEM_CLASS
		(gtk_type_class (gnome_canvas_item_get_type ()));
}

static void
rollback_control_set_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	RollbackControl *rollback_control;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ROLLBACK_CONTROL (object));

	rollback_control = ROLLBACK_CONTROL (object);

	switch (arg_id) {	case ARG_CONTROL_NUMBER:
		rollback_control->p->y =
			GTK_VALUE_INT (*arg) * CONTROL_HEIGHT;
		break;

	case ARG_BACKEND_ID:
		g_return_if_fail (GTK_VALUE_POINTER (*arg) != NULL);

		rollback_control->p->backend_id =
			g_strdup (GTK_VALUE_POINTER (*arg));
		break;

	case ARG_IS_GLOBAL:
		if (GTK_VALUE_INT (*arg))
			rollback_control->p->archive =
				ARCHIVE (archive_load (TRUE));
		else
			rollback_control->p->archive =
				ARCHIVE (archive_load (FALSE));

		rollback_control->p->location =
			archive_get_current_location
			(rollback_control->p->archive);
/*  		rollback_control->p->config_log = */
/*  			location_get_config_log */
/*  			(rollback_control->p->location); */
		break;

	default:
		g_warning ("Bad argument set");
		break;
	}
}

static void
rollback_control_get_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	RollbackControl *rollback_control;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ROLLBACK_CONTROL (object));

	rollback_control = ROLLBACK_CONTROL (object);

	switch (arg_id) {
	case ARG_CONTROL_NUMBER:
		GTK_VALUE_INT (*arg) = rollback_control->p->y / CONTROL_HEIGHT;
		break;

	case ARG_BACKEND_ID:
		GTK_VALUE_POINTER (*arg) = rollback_control->p->backend_id;
		break;

	case ARG_IS_GLOBAL:
		GTK_VALUE_INT (*arg) = rollback_control->p->archive->is_global;
		break;

	default:
		g_warning ("Bad argument get");
		break;
	}
}

static void
rollback_control_finalize (GtkObject *object) 
{
	RollbackControl *rollback_control;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ROLLBACK_CONTROL (object));

	rollback_control = ROLLBACK_CONTROL (object);

	g_free (rollback_control->p);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
rollback_control_update (GnomeCanvasItem *item, double affine[6],
			 ArtSVP *clip_path, gint flags) 
{
	RollbackControl *control;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_ROLLBACK_CONTROL (item));

	control = ROLLBACK_CONTROL (item);

	/* We don't support aa canvas for now */

	if (item->canvas->aa) {
		g_warning ("Anti-aliased canvas not supported for "
			   "this item");
	} else {
		/* FIXME: Fix this */
		gnome_canvas_update_bbox (item, 
					  0, control->p->y, 65536,
					  control->p->y + CONTROL_HEIGHT);
	}
}

static void
rollback_control_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
		       int x, int y, int width, int height)
{
	RollbackWidget *widget;
	RollbackControl *control;
	GdkGC *gc;

	g_return_if_fail (item != NULL);
	g_return_if_fail (IS_ROLLBACK_CONTROL (item));
	g_return_if_fail (IS_ROLLBACK_WIDGET (item->canvas));

	control = ROLLBACK_CONTROL (item);
	widget = ROLLBACK_WIDGET (item->canvas);

	gc = rollback_widget_get_gc (widget);
	control->p->drawable = drawable;

	/* Render the background color */
	gdk_gc_set_foreground (gc, &widget->control_colors[BACKGROUND_COLOR]);
	gdk_draw_rectangle (drawable, gc, TRUE, x, y, width, height);

	/* Render the markers */
	gdk_gc_set_foreground (gc, &widget->control_colors[MARKER_COLOR]);
	config_log_iterate (control->p->config_log,
			    (ConfigLogIteratorCB) draw_markers_cb, control);
}

static int
draw_markers_cb (ConfigLog *config_log, gint id, gchar *backend_id,
		 struct tm *time, RollbackControl *control) 
{
	RollbackWidget *widget;
	GdkGC *gc;
	gint x;

	if (strcmp (backend_id, control->p->backend_id)) return 0;

	widget = ROLLBACK_WIDGET (GNOME_CANVAS_ITEM (control)->canvas);
	gc = rollback_widget_get_gc (widget);	

	/* FIXME: Get the width correctly */
	x = time_to_x (mktime (time), 300);

	gdk_draw_arc (control->p->drawable, gc, TRUE, x,
		      control->p->y + (CONTROL_HEIGHT - MARKER_SIZE) / 2,
		      MARKER_SIZE, MARKER_SIZE, 0, 360 * 64);

	return 0;
}

/* I think I'm going to fake this over the rationals here by getting the lcm
 * of all the denominators and dividing the result by that in the end. That
 * might make my life less painful.
 */

static gint
time_to_x (time_t t, gint width)
{
	gint coeff[5];   /* We have five points, so we use a degree-4
			  * polynomial */
	time_t now;

	now = time (NULL);

	/* FIXME: Find out the real coefficients to use here */
	coeff[0] = 1;
	coeff[1] = 2;
	coeff[2] = 2;
	coeff[3] = 5;
	coeff[4] = 6;

	return width - width * (now - t) / (7 * 24 * 60 * 60);
/*  	return width - horner (coeff, 1, 4, now - t) * width; */
}

/* Treat the array coeff as the coefficient vector of a polynomial over the
 * ring of integers and evaluate that polynomial at the given integer x,
 * dividing at each step by the given divisor
 */

static gint
horner (gint *coeff, gint divisor, gint degree, gint x)
{
	gint total = 0, i;

	for (i = 0; i <= degree; i++)
		total = (total * x + coeff[i]) / divisor;

	return total;
}
