/*
 *  Authors: Rodney Dawes <dobey@ximian.com>
 *
 *  Copyright 2003 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef _GNOME_WP_UTILS_H_
#define _GNOME_WP_UTILS_H_

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

GdkPixbuf * gnome_wp_pixbuf_new_gradient (GtkOrientation orientation,
					  GdkColor * c1,
					  GdkColor * c2,
					  gint width, gint height);
GdkPixbuf * gnome_wp_pixbuf_new_solid (GdkColor * color,
				       gint width, gint height);
GdkPixbuf * gnome_wp_pixbuf_tile (GdkPixbuf * src_pixbuf,
				  GdkPixbuf * dest_pixbuf);
GdkPixbuf * gnome_wp_pixbuf_center (GdkPixbuf * src_pixbuf,
				    GdkPixbuf * dest_pixbuf);

G_END_DECLS

#endif
