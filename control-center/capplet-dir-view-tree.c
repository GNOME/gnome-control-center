/* -*- mode: c; style: linux -*- */

/* capplet-dir-view-tree.c
 * Copyright (C) 2000, 2001 Ximian, Inc.
 *
 * Authors: Bradford Hovinen <hovinen@ximian.com>
 *          Jacob Berkman <jacob@ximian.com>
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

#include "capplet-dir-view.h"

static void
tree_clear (CappletDirView *view)
{
	g_return_if_fail (GTK_IS_CTREE (view->view_data));

	gtk_clist_clear (GTK_CLIST (view->view_data));
}

static void
tree_clean (CappletDirView *view)
{
	g_return_if_fail (GTK_IS_CTREE (view->view_data));

	view->view_data = NULL;
}

static void
populate_tree_branch (CappletDir *dir, GtkCTree *ctree, GtkCTreeNode *parent) 
{
	CappletDirEntry *entry;
	GdkPixbuf *pixbuf, *scaled;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GtkCTreeNode *current = NULL;
	GSList *list;

	capplet_dir_load (dir);

	for (list = dir->entries; list; list = list->next) {
		entry = CAPPLET_DIR_ENTRY (list->data);

		pixbuf = gdk_pixbuf_new_from_file (entry->icon, NULL);
		scaled = gdk_pixbuf_scale_simple (pixbuf, 16, 16, 
						  GDK_INTERP_BILINEAR);
		gdk_pixbuf_render_pixmap_and_mask (scaled, &pixmap, &mask, 128);
		gdk_pixbuf_unref (pixbuf);
		gdk_pixbuf_unref (scaled);

#warning Should an array be created instead of passing &entry->label ?
		current = gtk_ctree_insert_node 
			(ctree, parent, NULL,
			 &entry->label, 10,
			 pixmap, mask, pixmap, mask,
			 IS_CAPPLET (entry), FALSE);
		gtk_ctree_node_set_row_data (ctree, current, entry);

		if (IS_CAPPLET_DIR (entry))
			populate_tree_branch (CAPPLET_DIR (entry), ctree, current);
	}
}

static void
tree_populate (CappletDirView *view) 
{
	g_return_if_fail (GTK_IS_CTREE (view->view_data));

	gtk_clist_freeze (GTK_CLIST (view->view_data));
	populate_tree_branch (view->capplet_dir, GTK_CTREE (view->view_data), NULL);
	gtk_clist_thaw (GTK_CLIST (view->view_data));
}

static void
select_tree_cb (GtkCTree *ctree, GtkCTreeNode *node, gint column,
		GdkEventButton *event, CappletDirView *view) 
{
	CappletDirEntry *dir_entry;

	dir_entry = gtk_ctree_node_get_row_data (ctree, node);
	view->selected = dir_entry;
}

static gint
tree_event_cb (GtkCTree *ctree, GdkEventButton *event,
	       CappletDirView *view) 
{
	CappletDirEntry *entry;
	GtkCTreeNode *node;
	gint row, column;

	if (event->type == GDK_2BUTTON_PRESS && event->button == 1) 
	{
		gtk_clist_get_selection_info (GTK_CLIST (ctree),
					      event->x, event->y,
					      &row, &column);
		node = gtk_ctree_node_nth (ctree, row);
		entry = gtk_ctree_node_get_row_data (ctree, node);
		if (entry && IS_CAPPLET (entry))
			capplet_dir_entry_activate (entry, view);
	}
	return FALSE;
}

static GtkWidget *
tree_create (CappletDirView *view) 
{
	GtkAdjustment *adjustment;
	GtkWidget *w, *sw;
	
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	view->view_data = w = gtk_ctree_new (1, 0);

	gtk_signal_connect (GTK_OBJECT (w), "tree-select-row",
			    GTK_SIGNAL_FUNC (select_tree_cb),
			    view);
	gtk_signal_connect (GTK_OBJECT (w), "button_press_event", 
			    GTK_SIGNAL_FUNC (tree_event_cb),
			    view);

	gtk_container_add (GTK_CONTAINER (sw), w);
	gtk_widget_show_all (sw);

	return sw;
}

#if 0
static void
switch_to_tree (CappletDirView *view) 
{
	CappletDir *dir, *old_dir = NULL;
	GtkCTreeNode *node;

	if (view->layout != LAYOUT_TREE) {
		if (view->capplet_dir) {
			old_dir = view->capplet_dir;

			while (CAPPLET_DIR_ENTRY (view->capplet_dir)->dir)
				view->capplet_dir = CAPPLET_DIR_ENTRY 
					(view->capplet_dir)->dir;
		}

		clean (view);
		create_tree (view);
		view->layout = LAYOUT_TREE;

		if (!view->capplet_dir) return;

		if (view->selected) {
			node = gtk_ctree_find_by_row_data (view->u.tree,
							   NULL,
							   view->selected);

			gtk_ctree_select (view->u.tree, node);

			dir = IS_CAPPLET_DIR (view->selected) ? 
				CAPPLET_DIR (view->selected) :
				view->selected->dir;
		} else {
			dir = old_dir;
		}

		while (dir) {
			node = gtk_ctree_find_by_row_data (view->u.tree,
							   NULL, dir);
			if (!node) break;
			gtk_ctree_expand (view->u.tree, node);
			dir = CAPPLET_DIR_ENTRY (dir)->dir;
		}
	}
}
#endif

CappletDirViewImpl capplet_dir_view_tree = {
	tree_clear,
	tree_clean,
	tree_populate,
	tree_create
};

