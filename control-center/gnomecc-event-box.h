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
/* Event Box item type for GnomeCanvas widget
 *
 * GnomeCanvas is basically a port of the Tk toolkit's most excellent canvas widget.  Tk is
 * copyrighted by the Regents of the University of California, Sun Microsystems, and other parties.
 *
 *
 * Author: Chris Lahey <clahey@ximian.com>
 */

#ifndef GNOMECC_EVENT_BOX_H
#define GNOMECC_EVENT_BOX_H


#include <libgnomecanvas/gnome-canvas.h>

#include <libgnomecanvas/gnome-canvas-rect-ellipse.h>

G_BEGIN_DECLS

/* Event Box item.  No configurable or queryable arguments are available (use those in
 * GnomeCanvasRE).
 */


#define GNOMECC_TYPE_EVENT_BOX            (gnomecc_event_box_get_type ())
#define GNOMECC_EVENT_BOX(obj)            (GTK_CHECK_CAST ((obj), GNOMECC_TYPE_EVENT_BOX, GnomeccEventBox))
#define GNOMECC_EVENT_BOX_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), GNOMECC_TYPE_EVENT_BOX, GnomeccEventBoxClass))
#define GNOMECC_IS_EVENT_BOX(obj)         (GTK_CHECK_TYPE ((obj), GNOMECC_TYPE_EVENT_BOX))
#define GNOMECC_IS_EVENT_BOX_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), GNOMECC_TYPE_EVENT_BOX))
#define GNOMECC_EVENT_BOX_GET_CLASS(obj)  (GTK_CHECK_GET_CLASS ((obj), GNOMECC_TYPE_EVENT_BOX, GnomeccEventBoxClass))


typedef struct _GnomeccEventBox      GnomeccEventBox;
typedef struct _GnomeccEventBoxClass GnomeccEventBoxClass;

struct _GnomeccEventBox {
	GnomeCanvasRect item;
};

struct _GnomeccEventBoxClass {
	GnomeCanvasRectClass parent_class;
};


/* Standard Gtk function */
GType gnomecc_event_box_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
