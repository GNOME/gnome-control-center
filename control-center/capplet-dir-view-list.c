/* -*- mode: c; style: linux -*- */

/* capplet-dir-view-list.c
 * Copyright (C) 2000, 2001 Ximian, Inc.
 *
 * Authors: Jacob Berkman <jacob@ximian.com>
 *          Bradford Hovinen <hovinen@ximian.com>
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

#include <config.h>

#include <gdk-pixbuf/gnome-canvas-pixbuf.h>

#include "capplet-dir-view.h"

static void
list_clear (CappletDirView *view)
{
	g_return_if_fail (GNOME_IS_ICON_LIST (view->view_data));

	gnome_icon_list_clear (GNOME_ICON_LIST (view->view_data));
}

static void
list_clean (CappletDirView *view)
{
	/* i think this can be a no-op now */
	view->view_data = NULL;
}

/*
 * Creates a 24-bits RGB value from a GdkColor
 */
static guint
rgb_from_gdk_color (GdkColor *color)
{
	guint a =
		(((color->red >> 8) << 16) |
		 ((color->green >> 8) << 8) |
		 ((color->blue >> 8)));
	
	return a;
}

static GnomeCanvasItem *
flatten_alpha (GdkPixbuf *image, GnomeCanvas *canvas)
{
	GnomeCanvasItem *item;
	GtkStyle *style;
	GdkPixbuf *flat;
	guint rgb;
	
	if (!image || !gdk_pixbuf_get_has_alpha (image))
		return NULL;

	if (!GTK_WIDGET_REALIZED (GTK_WIDGET (canvas)))
		gtk_widget_realize (GTK_WIDGET (canvas));
	
	style = gtk_widget_get_style (GTK_WIDGET (canvas));
	rgb = rgb_from_gdk_color (&style->base[GTK_STATE_NORMAL]);
	
	flat = gdk_pixbuf_composite_color_simple (
		image,
		gdk_pixbuf_get_width (image),
		gdk_pixbuf_get_height (image),
		GDK_INTERP_NEAREST,
		255,
		32,
		rgb, rgb);

	item = gnome_canvas_item_new (GNOME_CANVAS_GROUP (canvas->root),
				      GNOME_TYPE_CANVAS_PIXBUF,
				      "pixbuf", flat, 
				      "height", (double)gdk_pixbuf_get_height (flat),
				      "width", (double)gdk_pixbuf_get_width (flat),
				      NULL);

	gdk_pixbuf_unref (flat);

	return item;
}

static void 
list_populate (CappletDirView *view) 
{
	GSList *list;
	int i;
	GnomeCanvasItem *item;

	g_return_if_fail (GNOME_IS_ICON_LIST (view->view_data));

	gnome_icon_list_freeze (GNOME_ICON_LIST (view->view_data));

	for (i = 0, list = view->capplet_dir->entries; list; list = list->next, i++) {
#if 0
		item = flatten_alpha (CAPPLET_DIR_ENTRY (list->data)->pb,
				      GNOME_CANVAS (view->view_data));
		gnome_icon_list_insert_item (GNOME_ICON_LIST (view->view_data), i, item, 
					     CAPPLET_DIR_ENTRY (list->data)->label);
#else
		gnome_icon_list_insert (GNOME_ICON_LIST (view->view_data), i,
					CAPPLET_DIR_ENTRY (list->data)->icon,
					CAPPLET_DIR_ENTRY (list->data)->label);
#endif
	}
	gnome_icon_list_thaw (GNOME_ICON_LIST (view->view_data));
}

static void 
select_icon_list_cb (GtkWidget *widget, gint arg1, GdkEvent *event, 
		     CappletDirView *view) 
{
	if (event->type == GDK_2BUTTON_PRESS &&
	    ((GdkEventButton *) event)->button == 1) 
	{
		capplet_dir_entry_activate 
			(g_slist_nth_data (view->capplet_dir->entries, arg1), view);
		view->selected = NULL;
	} else {
		view->selected = g_slist_nth_data (view->capplet_dir->entries, arg1);
	}
}

static GtkWidget *
list_create (CappletDirView *view) 
{
	GtkAdjustment *adjustment;
	GtkWidget *w, *sw;
	GSList *list;
	int i;

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (sw));		

	view->view_data = w = gnome_icon_list_new (72, adjustment, 0);

	if (view->selected)
		view->capplet_dir = view->selected->dir;

#if 0
	if (view->capplet_dir) populate_icon_list (view);

	if (view->selected) {
		for (i = 0, list = view->capplet_dir->entries; list; i++, list = list->next) {
			if (list->data == view->selected) {
				gnome_icon_list_select_icon (view->u.icon_list, i);	
				break;
			}
		}
	}
#endif

	gtk_signal_connect (GTK_OBJECT (w), "select-icon", 
			    GTK_SIGNAL_FUNC (select_icon_list_cb),
			    view);

	gtk_container_add (GTK_CONTAINER (sw), w);
	gtk_widget_show_all (sw);

	return sw;
}

CappletDirViewImpl capplet_dir_view_list = {
	list_clear,
	list_clean,
	list_populate,
	list_create
};
