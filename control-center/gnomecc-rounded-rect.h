/* vim: set sw=8: -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * gnomecc-rounded-rect.c: A rectangle with rounded corners
 *
 * Copyright (C) 2004 Novell Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */
#ifndef GNOMECC_ROUNDED_RECT_H
#define GNOMECC_ROUNDED_RECT_H

#include <libgnomecanvas/gnome-canvas.h>
#include <libgnomecanvas/gnome-canvas-rect-ellipse.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GNOMECC_TYPE_ROUNDED_RECT	(gnomecc_rounded_rect_get_type ())
#define GNOMECC_ROUNDED_RECT(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GNOMECC_TYPE_ROUNDED_RECT, GnomeccRoundedRect))
#define GNOMECC_IS_ROUNDED_RECT(o)	(G_TYPE_CHECK_INSTANCE_TYPE ((o), GNOMECC_TYPE_ROUNDED_RECT))

typedef struct _GnomeccRoundedRect GnomeccRoundedRect;
GType gnomecc_rounded_rect_get_type (void);

G_END_DECLS

#endif /* GNOMECC_ROUNDED_RECT_H */
