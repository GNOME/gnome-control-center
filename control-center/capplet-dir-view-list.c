/* -*- mode: c; style: linux -*- */

/* capplet-dir-view-list.c
 * Copyright (C) 2000, 2001 Ximian, Inc.
 *
 * Authors: Jacob Berkman <jacob@ximian.com>
 *          Bradford Hovinen <hovinen@ximian.com>
 *          Richard Hestilow <hestilow@ximian.com>
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

#include <libgnomecanvas/gnome-canvas.h>
#include <gtk/gtk.h>

#include "capplet-dir-view.h"

typedef struct
{
	GnomeIconList *gil;
	CappletDir *root_dir;
	CappletDir *current;
	gboolean ignore_selection;

	/* Header stuff */
	GtkWidget *header;
	GdkPixbuf *header_logo;
	GdkGC *gc1, *gc2;
	PangoLayout *layout;
	gchar *header_text;

	/* Sidebar */
	GtkListStore *sidebar_model;
	GtkWidget *sidebar;
	gboolean sidebar_populated;
	GdkPixbuf *arrow;
} ListViewData;

enum
{
	SIDEBAR_ICON,
	SIDEBAR_LABEL,
	SIDEBAR_ACTIVE,
	SIDEBAR_DATA
};

static void
list_clear (CappletDirView *view)
{
	ListViewData *data = view->view_data;

	g_return_if_fail (GNOME_IS_ICON_LIST (data->gil));

	gnome_icon_list_clear (data->gil);
}

static void
list_clean (CappletDirView *view)
{
	ListViewData *data = view->view_data;

	gdk_pixbuf_unref (data->header_logo);
	gdk_pixbuf_unref (data->arrow);
	gdk_gc_unref (data->gc1);
	gdk_gc_unref (data->gc2);

	g_free (data);
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

static gpointer
real_slist_nth_data (GSList *list, guint n, guint type)
{
	int i = 0;
	
	for (; list; list = list->next)
	{
		if (CAPPLET_DIR_ENTRY (list->data)->type != type)
			continue;
		if (i == n)
			return list->data;
		i++;
	}
}

static void
sidebar_dummy_foreach (GtkTreeModel *model, GtkTreePath *path,
		       GtkTreeIter *iter, CappletDir **dir)
{
	GValue val = {0, };
	
	g_return_if_fail (dir != NULL);

	g_value_init (&val, G_TYPE_POINTER);
	gtk_tree_model_get_value (model, iter, SIDEBAR_DATA, &val);
	*dir = g_value_get_pointer (&val);
}

static void
sidebar_select_cb (GtkTreeSelection *sel, CappletDirView *view)
{
	ListViewData *data = view->view_data;
	CappletDir *dir;

	if (data->ignore_selection)
		return;

	gtk_tree_selection_selected_foreach (sel, sidebar_dummy_foreach, &dir);
	data->current = dir;
	capplet_dir_entry_activate (CAPPLET_DIR_ENTRY (dir), view);
}

static void
sidebar_arrow_update (CappletDirView *view)
{
	ListViewData *data = view->view_data;
	GSList *list, *root;
	GtkTreeIter iter;
	int enabled, i;

	g_print ("Hi\n");
	if (!data->arrow)
	{
		gchar *file = g_concat_dir_and_file (ART_DIR, "active.png");
		data->arrow = gdk_pixbuf_new_from_file (file, NULL);
	}

	root = list = g_slist_append (NULL, data->root_dir);
	list->next = data->root_dir->entries;

	for (enabled = 0; list; list = list->next)
	{
		CappletDirEntry *dir = CAPPLET_DIR_ENTRY (list->data);

		if (dir->type != TYPE_CAPPLET_DIR)
			continue;

		if (CAPPLET_DIR (dir) == data->current)
			break;

		enabled++;
	}

	g_print ("okay\n");
	for (i = 0, list = root; list; list = list->next)
	{
		CappletDirEntry *dir = CAPPLET_DIR_ENTRY (list->data);

		if (dir->type != TYPE_CAPPLET_DIR)
			continue;
		
		gtk_tree_model_iter_nth_child 
			(GTK_TREE_MODEL (data->sidebar_model),
			 &iter, NULL, i);
		gtk_list_store_set (data->sidebar_model, &iter,
				    SIDEBAR_ACTIVE, (i == enabled) ? data->arrow : NULL,
				    -1);
		i++;
	}

	g_slist_free_1 (root);
	g_print ("right\n");
}

static void
sidebar_populate (CappletDirView *view)
{
	ListViewData *data = view->view_data;
	GSList *list, *root;

	g_print ("Hi 2\n");
	if (data->sidebar_populated)
	{
		sidebar_arrow_update (view);
		return;
	}

	gtk_list_store_clear (data->sidebar_model);
	
	root = list = g_slist_append (NULL, data->root_dir);
	list->next = data->root_dir->entries;

	for (; list; list = list->next)
	{
		GdkPixbuf *buf;
		GtkTreeIter iter;
		CappletDirEntry *dir = CAPPLET_DIR_ENTRY (list->data);

		if (dir->type != TYPE_CAPPLET_DIR)
			continue;
		
		buf = gdk_pixbuf_new_from_file (dir->icon, NULL);
		
		gtk_list_store_append (data->sidebar_model, &iter);
		gtk_list_store_set (data->sidebar_model, &iter,
				    SIDEBAR_ICON, buf,
				    SIDEBAR_LABEL, dir->label,
				    SIDEBAR_DATA, dir,
				    SIDEBAR_ACTIVE, NULL,
				    -1);

		gdk_pixbuf_unref (buf);
	}

	g_slist_free_1 (root); /* Just this first node */
	data->sidebar_populated = TRUE;

	sidebar_arrow_update (view);
}

static void 
list_populate (CappletDirView *view) 
{
	GSList *list;
	int i;
	GnomeCanvasItem *item;
	GtkTreeIter iter;
	ListViewData *data = view->view_data;

	g_return_if_fail (GNOME_IS_ICON_LIST (data->gil));

	if (!data->root_dir)
		data->root_dir = view->capplet_dir;
	
	data->ignore_selection = TRUE;
	sidebar_populate (view);

	/* FIXME: Is this triggering a gtk+ bug? */
#if 0
	gtk_tree_model_get_iter_root (GTK_TREE_MODEL (data->sidebar_model),
		&iter);
	gtk_tree_selection_select_iter (
		gtk_tree_view_get_selection (GTK_TREE_VIEW (data->sidebar)),
		&iter);
#endif
	data->ignore_selection = FALSE;
	
	gnome_icon_list_freeze (data->gil);

	for (i = 0, list = view->capplet_dir->entries; list; list = list->next)
	{
                if (CAPPLET_DIR_ENTRY (list->data)->type == TYPE_CAPPLET_DIR)
			continue;
		
#if 0
		item = flatten_alpha (CAPPLET_DIR_ENTRY (list->data)->pb,
				      GNOME_CANVAS (view->view_data));
		gnome_icon_list_insert_item (GNOME_ICON_LIST (view->view_data), i, item, 
					     CAPPLET_DIR_ENTRY (list->data)->label);
#else
		g_print ("Icon: %s %s", CAPPLET_DIR_ENTRY (list->data)->icon,
					CAPPLET_DIR_ENTRY (list->data)->label);
		gnome_icon_list_insert (data->gil, i++,
					CAPPLET_DIR_ENTRY (list->data)->icon,
					CAPPLET_DIR_ENTRY (list->data)->label);
#endif
	}
	gnome_icon_list_thaw (data->gil);

	g_free (data->header_text);
	data->header_text = g_strdup_printf (_("GNOME Control Center: %s"),
		CAPPLET_DIR_ENTRY (view->capplet_dir)->label);
	gtk_widget_queue_draw (data->header);
}

static void 
select_icon_list_cb (GtkWidget *widget, gint arg1, GdkEvent *event, 
		     CappletDirView *view) 
{
	if (event->type == GDK_2BUTTON_PRESS &&
	    ((GdkEventButton *) event)->button == 1) 
	{
		capplet_dir_entry_activate 
			(real_slist_nth_data (view->capplet_dir->entries, arg1, TYPE_CAPPLET), view);
		view->selected = NULL;
	} else {
		view->selected = real_slist_nth_data (view->capplet_dir->entries, arg1, TYPE_CAPPLET);
	}
}

void
lighten_color (GdkColor *color)
{
	g_return_if_fail (color != NULL);
#define SPLIT_THE_DISTANCE(x, y) ((x) += ((y) - (x)) / 2)
	SPLIT_THE_DISTANCE (color->red, 1 << 16);
	SPLIT_THE_DISTANCE (color->green, 1 << 16);
	SPLIT_THE_DISTANCE (color->blue, 1 << 16);
#undef SPLIT_THE_DISTANCE
}

static gint 
header_expose_cb (GtkWidget *darea, GdkEventExpose *event,
		  CappletDirView *view)
{
	ListViewData *data = view->view_data;
	int tw, th;

	int i;

	if (!data->gc1)
	{
		GdkColor c1, c2;

		//gdk_color_parse ("#625784", &c1);
		c1 = darea->style->bg[GTK_STATE_SELECTED];
		lighten_color (&c1);
		//gdk_color_parse ("#494066", &c2);
		//c2 = darea->style->dark[GTK_STATE_SELECTED];
		c2 = darea->style->black;
		lighten_color (&c2);

		data->gc1 = gdk_gc_new (darea->window);
		gdk_gc_copy (data->gc1, darea->style->white_gc);
		gdk_gc_set_rgb_fg_color (data->gc1, &c1);
		
		data->gc2 = gdk_gc_new (darea->window);
		gdk_gc_copy (data->gc2, darea->style->white_gc);
		gdk_gc_set_rgb_fg_color (data->gc2, &c2);
	}

	if (!data->layout)
	{
		PangoContext *context = gtk_widget_get_pango_context (darea);
		PangoFontDescription *desc = pango_font_description_copy (darea->style->font_desc);
		pango_font_description_set_size (desc, 16);
		pango_font_description_set_weight (desc, PANGO_WEIGHT_BOLD);
		data->layout = pango_layout_new (context);
		pango_layout_set_font_description (data->layout, desc);
		pango_font_description_free (desc);
	}
	
	for (i = 0; i < event->area.height; i++)
	{
		int y = i + event->area.y;
		GdkGC *gc = (y % 2) ? data->gc2 : data->gc1;
		gdk_draw_line (darea->window, gc,
		 	       event->area.x, y,
			       event->area.x + event->area.width,
			       y);
	}
	
	gdk_pixbuf_render_to_drawable_alpha (data->header_logo,
		darea->window,
		event->area.x, event->area.y,
		event->area.x, event->area.y,
		MIN (gdk_pixbuf_get_width (data->header_logo), event->area.width),
		MIN (gdk_pixbuf_get_height (data->header_logo), event->area.height),
		GDK_PIXBUF_ALPHA_FULL, 255,
		GDK_RGB_DITHER_MAX,
		0, 0);

	pango_layout_set_text (data->layout, data->header_text, -1);
	pango_layout_get_pixel_size (data->layout, &tw, &th);
//	gdk_draw_layout (darea->window, darea->style->text[GTK_STATE_SELECTED],
	gdk_draw_layout (darea->window, darea->style->white_gc,
			 64, (darea->allocation.height - th) / 2,
			 data->layout);

	return TRUE;
}

static GtkWidget *
list_create (CappletDirView *view) 
{
	GtkAdjustment *adjustment;
	GtkWidget *w, *sw, *vbox, *hbox;
	GtkWidget *darea;
	GSList *list;
	ListViewData *data;
	gchar *title;
	int i;
	GtkCellRenderer *renderer;
	GtkTreeSelection *sel;

	data = view->view_data = g_new0 (ListViewData, 1);

	vbox = gtk_vbox_new (FALSE, 0);
	darea = data->header =  gtk_drawing_area_new ();
	gtk_widget_set_usize (darea, 48, 48);
	gtk_signal_connect (GTK_OBJECT (darea), "expose_event",
			    header_expose_cb, view);

	gtk_box_pack_start (GTK_BOX (vbox), darea, FALSE, FALSE, 0);
		
	hbox = gtk_hpaned_new ();
	
	data->sidebar_model = gtk_list_store_new (4,
		GDK_TYPE_PIXBUF, G_TYPE_STRING, GDK_TYPE_PIXBUF,
		G_TYPE_POINTER);

	data->sidebar = w = gtk_tree_view_new_with_model (GTK_TREE_MODEL (data->sidebar_model));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (w), FALSE);
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (w),
		-1, "", renderer, "pixbuf", SIDEBAR_ICON, NULL);
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (w),
		-1, "", renderer, "text", SIDEBAR_LABEL, NULL);
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (w),
		-1, "", renderer, "pixbuf", SIDEBAR_ACTIVE, NULL);

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (w));
	gtk_tree_selection_set_mode (sel, GTK_SELECTION_BROWSE);
	g_signal_connect (G_OBJECT (sel), "changed", sidebar_select_cb, view);

	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_set_usize (sw, 200, -1);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (sw), w);
	gtk_paned_add1 (GTK_PANED (hbox), sw);
			
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (sw));		

	data->gil = w = gnome_icon_list_new (72, NULL, 0);
	
	title = g_concat_dir_and_file (ART_DIR, "title.png");
	data->header_logo = gdk_pixbuf_new_from_file (title, NULL);
	g_free (title);

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
	gtk_paned_add2 (GTK_PANED (hbox), sw);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
	gtk_widget_show_all (vbox);
	
	return vbox;
}

CappletDirViewImpl capplet_dir_view_list = {
	list_clear,
	list_clean,
	list_populate,
	list_create
};
