/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002 Free Software Foundation
 * All rights reserved.
 *
 * This file is part of the Gnome Library.
 *
 * The Gnome Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The Gnome Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the Gnome Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
/*
  @NOTATION@
 */
/* Event box item type for GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
 *
 *
 * Author: Chris Lahey <clahey@ximian.com>
 */

#include <config.h>

#include "gnomecc-event-box.h"

#define noVERBOSE

static GnomeCanvasItemClass *parent_class;

static double
gnomecc_event_box_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
			 GnomeCanvasItem **actual_item)
{
	double x1, x2, y1, y2;
	g_object_get (item,
		      "x1", &x1,
		      "x2", &x2,
		      "y1", &y1,
		      "y2", &y2,
		      NULL);
	if (x <= x2 && x >= x1 &&
	    y <= y2 && y >= y1) {
		*actual_item = item;
		
		return 0.0;
	}
	return 1e12;
}

static void
gnomecc_event_box_class_init (GnomeccEventBoxClass *class)
{
	GnomeCanvasItemClass *item_class;

	item_class = (GnomeCanvasItemClass *) class;

	parent_class = g_type_class_peek_parent (class);

	item_class->point = gnomecc_event_box_point;
}

GType
gnomecc_event_box_get_type (void)
{
	static GType type;

	if (!type) {
		static const GTypeInfo object_info = {
			sizeof (GnomeccEventBoxClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gnomecc_event_box_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,			/* class_data */
			sizeof (GnomeccEventBox),
			0,			/* n_preallocs */
			(GInstanceInitFunc) NULL,
			NULL			/* value_table */
		};

		type = g_type_register_static (GNOME_TYPE_CANVAS_RECT, "GnomeccEventBox",
					       &object_info, 0);
	}

	return type;
}
