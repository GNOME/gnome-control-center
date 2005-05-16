/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Copyright (C) 2005 Carlos Garnacho
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Jody Goldberg          <jody@gnome.org>
 *          Carlos Garnacho Parro  <carlosg@gnome.org>
 */

#ifndef GNOMECC_CANVAS_H
#define GNOMECC_CANVAS_H

#include <libgnomecanvas/gnome-canvas.h>
#include "control-center-categories.h"

G_BEGIN_DECLS


#define GNOMECC_TYPE_CANVAS         (gnomecc_canvas_get_type ())
#define GNOMECC_CANVAS(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GNOMECC_TYPE_CANVAS, GnomeccCanvas))
#define GNOMECC_CANVAS_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST    ((c), GNOMECC_TYPE_CANVAS, GnomeccCanvasClass))
#define GNOMECC_IS_CANVAS(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOMECC_TYPE_CANVAS))
#define GNOMECC_IS_CANVAS_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE    ((c), GNOMECC_TYPE_CANVAS))
#define GNOMECC_CANVAS_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS  ((o), GNOMECC_TYPE_CANVAS, GnomeccCanvasClass))


typedef struct _GnomeccCanvas      GnomeccCanvas;
typedef struct _GnomeccCanvasClass GnomeccCanvasClass;

struct _GnomeccCanvas {
	GnomeCanvas parent;
};

struct _GnomeccCanvasClass {
	GnomeCanvasClass parent_class;

	void (*changed) (GnomeccCanvas *canvas, gchar *str);
};


GType gnomecc_canvas_get_type (void);

GtkWidget* gnomecc_canvas_new (ControlCenterInformation *info);


G_END_DECLS

#endif /* GNOMECC_CANVAS_H */
