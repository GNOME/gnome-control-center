/* gnome-region-panel-xkblt.c
 * Copyright (C) 2003-2007 Sergey V. Udaltsov
 *
 * Written by: Sergey V. Udaltsov <svu@gnome.org>
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
#  include <config.h>
#endif

#include <gdk/gdkx.h>
#include <glib/gi18n.h>

#include <libgnomekbd/gkbd-desktop-config.h>
#include <libgnomekbd/gkbd-keyboard-drawing.h>

#include "gnome-region-panel-xkb.h"

enum {
	SEL_LAYOUT_TREE_COL_DESCRIPTION,
	SEL_LAYOUT_TREE_COL_ID,
	SEL_LAYOUT_TREE_COL_ENABLED
};

static int idx2select = -1;
static int max_selected_layouts = -1;
static int default_group = -1;

static GtkCellRenderer *text_renderer;

static gboolean disable_buttons_sensibility_update = FALSE;

static gint
find_selected_layout_idx (GtkBuilder * dialog)
{
	GtkTreeSelection *selection =
	    gtk_tree_view_get_selection (GTK_TREE_VIEW
					 (WID ("xkb_layouts_selected")));
	GtkTreeIter selected_iter;
	GtkTreeModel *model;
	GtkTreePath *path;
	gint *indices;
	gint rv;

	if (!gtk_tree_selection_get_selected
	    (selection, &model, &selected_iter))
		return -1;

	path = gtk_tree_model_get_path (model, &selected_iter);
	if (path == NULL)
		return -1;

	indices = gtk_tree_path_get_indices (path);
	rv = indices[0];
	gtk_tree_path_free (path);
	return rv;
}

gchar **
xkb_layouts_get_selected_list (void)
{
	gchar **retval;

	retval = g_settings_get_strv (xkb_keyboard_settings,
				      GKBD_KEYBOARD_CONFIG_KEY_LAYOUTS);
	if (retval == NULL || retval[0] == NULL) {
		g_strfreev (retval);
		retval = g_strdupv (initial_config.layouts_variants);
	}

	return retval;
}

gint
xkb_get_default_group ()
{
	return g_settings_get_int (xkb_desktop_settings,
				   GKBD_DESKTOP_CONFIG_KEY_DEFAULT_GROUP);
}

void
xkb_save_default_group (gint default_group)
{
	if (default_group != xkb_get_default_group ())
		g_settings_set_int (xkb_desktop_settings,
				    GKBD_DESKTOP_CONFIG_KEY_DEFAULT_GROUP,
				    default_group);
}

static void
xkb_layouts_enable_disable_buttons (GtkBuilder * dialog)
{
	GtkWidget *add_layout_btn = WID ("xkb_layouts_add");
	GtkWidget *show_layout_btn = WID ("xkb_layouts_show");
	GtkWidget *del_layout_btn = WID ("xkb_layouts_remove");
	GtkWidget *selected_layouts_tree = WID ("xkb_layouts_selected");
	GtkWidget *move_up_layout_btn = WID ("xkb_layouts_move_up");
	GtkWidget *move_down_layout_btn = WID ("xkb_layouts_move_down");

	GtkTreeSelection *s_selection =
	    gtk_tree_view_get_selection (GTK_TREE_VIEW
					 (selected_layouts_tree));
	const int n_selected_selected_layouts =
	    gtk_tree_selection_count_selected_rows (s_selection);
	GtkTreeModel *selected_layouts_model = gtk_tree_view_get_model
	    (GTK_TREE_VIEW (selected_layouts_tree));
	const int n_selected_layouts =
	    gtk_tree_model_iter_n_children (selected_layouts_model,
					    NULL);
	gint sidx = find_selected_layout_idx (dialog);

	if (disable_buttons_sensibility_update)
		return;

	gtk_widget_set_sensitive (add_layout_btn,
				  (n_selected_layouts <
				   max_selected_layouts
				   || max_selected_layouts == 0));
	gtk_widget_set_sensitive (del_layout_btn, (n_selected_layouts > 1)
				  && (n_selected_selected_layouts > 0));
	gtk_widget_set_sensitive (show_layout_btn,
				  (n_selected_selected_layouts > 0));
	gtk_widget_set_sensitive (move_up_layout_btn, sidx > 0);
	gtk_widget_set_sensitive (move_down_layout_btn, sidx >= 0
				  && sidx < (n_selected_layouts - 1));
}

static void
xkb_layouts_dnd_data_get (GtkWidget * widget, GdkDragContext * dc,
			  GtkSelectionData * selection_data, guint info,
			  guint t, GtkBuilder * dialog)
{
	/* Storing the value into selection -
	 * while it is actually not used
	 */
	gint idx = find_selected_layout_idx (dialog);
	gtk_selection_data_set (selection_data,
				GDK_SELECTION_TYPE_INTEGER, 32,
				(guchar *) & idx, sizeof (idx));
}

static void
xkb_layouts_dnd_data_received (GtkWidget * widget, GdkDragContext * dc,
			       gint x, gint y,
			       GtkSelectionData * selection_data,
			       guint info, guint t, GtkBuilder * dialog)
{
	gint sidx = find_selected_layout_idx (dialog);
	GtkWidget *tree_view = WID ("xkb_layouts_selected");
	GtkTreePath *path = NULL;
	GtkTreeViewDropPosition pos;
	gint didx;
	gchar *id;
	gchar **layouts_list;

	if (sidx == -1)
		return;

	layouts_list = xkb_layouts_get_selected_list ();
	id = g_strdup (layouts_list[sidx]);

	/* Remove the element at position sidx */
	gkbd_strv_behead (layouts_list + sidx);

	if (!gtk_tree_view_get_dest_row_at_pos
	    (GTK_TREE_VIEW (tree_view), x, y, &path, &pos)) {
		/* Move to the very end */
		int old_length = g_strv_length (layouts_list);
		layouts_list[old_length] = id;
		/* NULL was there before deletion, no need to re-assign */
		xkb_layouts_set_selected_list (layouts_list);
	} else if (path != NULL) {
		gint *indices = gtk_tree_path_get_indices (path);
		didx = indices[0];
		gtk_tree_path_free (path);
		/* Move to the new position */
		if (sidx != didx) {
			memmove (layouts_list + didx + 1,
				 layouts_list + didx,
				 g_strv_length (layouts_list + didx));
			layouts_list[didx] = id;
			xkb_layouts_set_selected_list (layouts_list);
		} else {
			g_free (id);
		}
	}
	g_strfreev (layouts_list);
}

void
xkb_layouts_prepare_selected_tree (GtkBuilder * dialog)
{
	GtkListStore *list_store =
	    gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING,
				G_TYPE_BOOLEAN);
	GtkWidget *tree_view = WID ("xkb_layouts_selected");
	GtkTreeSelection *selection;
	GtkTargetEntry self_drag_target =
	    { "xkb_layouts_selected", GTK_TARGET_SAME_WIDGET, 0 };
	GtkTreeViewColumn *desc_column;

	text_renderer = GTK_CELL_RENDERER (gtk_cell_renderer_text_new ());

	desc_column =
	    gtk_tree_view_column_new_with_attributes (_("Layout"),
						      text_renderer,
						      "text",
						      SEL_LAYOUT_TREE_COL_DESCRIPTION,
						      "sensitive",
						      SEL_LAYOUT_TREE_COL_ENABLED,
						      NULL);
	selection =
	    gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));

	gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view),
				 GTK_TREE_MODEL (list_store));

	gtk_tree_view_column_set_sizing (desc_column,
					 GTK_TREE_VIEW_COLUMN_AUTOSIZE);
	gtk_tree_view_column_set_resizable (desc_column, TRUE);
	gtk_tree_view_column_set_expand (desc_column, TRUE);

	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),
				     desc_column);

	g_signal_connect_swapped (G_OBJECT (selection), "changed",
				  G_CALLBACK
				  (xkb_layouts_enable_disable_buttons),
				  dialog);
	max_selected_layouts = xkl_engine_get_max_num_groups (engine);

	/* Setting up DnD */
	gtk_drag_source_set (tree_view, GDK_BUTTON1_MASK,
			     &self_drag_target, 1, GDK_ACTION_MOVE);
	gtk_drag_source_set_icon_name (tree_view, "input-keyboard");
	gtk_drag_dest_set (tree_view, GTK_DEST_DEFAULT_ALL,
			   &self_drag_target, 1, GDK_ACTION_MOVE);

	g_signal_connect (G_OBJECT (tree_view), "drag_data_get",
			  G_CALLBACK (xkb_layouts_dnd_data_get), dialog);
	g_signal_connect (G_OBJECT (tree_view), "drag_data_received",
			  G_CALLBACK (xkb_layouts_dnd_data_received),
			  dialog);
}

gchar *
xkb_layout_description_utf8 (const gchar * visible)
{
	char *l, *sl, *v, *sv;
	if (gkbd_keyboard_config_get_descriptions
	    (config_registry, visible, &sl, &l, &sv, &v))
		visible = gkbd_keyboard_config_format_full_layout (l, v);
	return g_strstrip (g_strdup (visible));
}

void
xkb_layouts_fill_selected_tree (GtkBuilder * dialog)
{
	gchar **layouts = xkb_layouts_get_selected_list ();
	guint i;
	GtkListStore *list_store =
	    GTK_LIST_STORE (gtk_tree_view_get_model
			    (GTK_TREE_VIEW
			     (WID ("xkb_layouts_selected"))));

	/* temporarily disable the buttons' status update */
	disable_buttons_sensibility_update = TRUE;

	gtk_list_store_clear (list_store);

	for (i = 0; layouts != NULL && layouts[i] != NULL; i++) {
		GtkTreeIter iter;
		char *cur_layout = layouts[i];
		gchar *utf_visible =
			xkb_layout_description_utf8 (cur_layout);

		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (list_store, &iter,
				    SEL_LAYOUT_TREE_COL_DESCRIPTION,
				    utf_visible,
				    SEL_LAYOUT_TREE_COL_ID,
				    cur_layout,
				    SEL_LAYOUT_TREE_COL_ENABLED,
				    i < max_selected_layouts,
				    -1);
		g_free (utf_visible);
	}

	g_strfreev (layouts);

	/* enable the buttons' status update */
	disable_buttons_sensibility_update = FALSE;

	if (idx2select != -1) {
		GtkTreeSelection *selection =
		    gtk_tree_view_get_selection ((GTK_TREE_VIEW
						  (WID
						   ("xkb_layouts_selected"))));
		GtkTreePath *path =
		    gtk_tree_path_new_from_indices (idx2select, -1);
		gtk_tree_selection_select_path (selection, path);
		gtk_tree_path_free (path);
		idx2select = -1;
	} else {
		/* if there is nothing to select - just enable/disable the buttons,
		   otherwise it would be done by the selection change */
		xkb_layouts_enable_disable_buttons (dialog);
	}
}

static void
add_selected_layout (GtkWidget * button, GtkBuilder * dialog)
{
	xkb_layout_choose (dialog);
}

static void
show_selected_layout (GtkWidget * button, GtkBuilder * dialog)
{
	gint idx = find_selected_layout_idx (dialog);

	if (idx != -1) {
		gchar **layouts_list = xkb_layouts_get_selected_list ();
		const gchar *id = layouts_list[idx];
		char *descr = xkb_layout_description_utf8 (id);
		GtkWidget *parent = WID ("region_dialog");
		GtkWidget *popup =
		    gkbd_keyboard_drawing_new_dialog (idx, descr);
		gtk_widget_set_parent (popup, parent);
		g_strfreev (layouts_list);
		g_free (descr);
	}
}

static void
remove_selected_layout (GtkWidget * button, GtkBuilder * dialog)
{
	gint idx = find_selected_layout_idx (dialog);

	if (idx != -1) {
		gchar **layouts_list = xkb_layouts_get_selected_list ();
		gkbd_strv_behead (layouts_list + idx);

		if (default_group > idx)
			xkb_save_default_group (default_group - 1);
		else if (default_group == idx)
			xkb_save_default_group (-1);

		xkb_layouts_set_selected_list (layouts_list);
		g_strfreev (layouts_list);
	}
}

static void
move_up_selected_layout (GtkWidget * button, GtkBuilder * dialog)
{
	gint idx = find_selected_layout_idx (dialog);

	if (idx != -1) {
		gchar **layouts_list = xkb_layouts_get_selected_list ();
		gchar *tmp = layouts_list[idx - 1];
		layouts_list[idx - 1] = layouts_list[idx];
		layouts_list[idx] = tmp;

		idx2select = idx - 1;
		xkb_layouts_set_selected_list (layouts_list);
		g_strfreev (layouts_list);
	}
}

static void
move_down_selected_layout (GtkWidget * button, GtkBuilder * dialog)
{
	gint idx = find_selected_layout_idx (dialog);

	if (idx != -1) {
		gchar **layouts_list = xkb_layouts_get_selected_list ();
		gchar *tmp = layouts_list[idx + 1];
		layouts_list[idx + 1] = layouts_list[idx];
		layouts_list[idx] = tmp;

		idx2select = idx + 1;
		xkb_layouts_set_selected_list (layouts_list);
		g_strfreev (layouts_list);
	}
}

void
xkb_layouts_register_buttons_handlers (GtkBuilder * dialog)
{
	g_signal_connect (G_OBJECT (WID ("xkb_layouts_add")), "clicked",
			  G_CALLBACK (add_selected_layout), dialog);
	g_signal_connect (G_OBJECT (WID ("xkb_layouts_show")), "clicked",
			  G_CALLBACK (show_selected_layout), dialog);
	g_signal_connect (G_OBJECT (WID ("xkb_layouts_remove")), "clicked",
			  G_CALLBACK (remove_selected_layout), dialog);
	g_signal_connect (G_OBJECT (WID ("xkb_layouts_move_up")),
			  "clicked", G_CALLBACK (move_up_selected_layout),
			  dialog);
	g_signal_connect (G_OBJECT (WID ("xkb_layouts_move_down")),
			  "clicked",
			  G_CALLBACK (move_down_selected_layout), dialog);
}

static void
xkb_layouts_update_list (GSettings * settings,
			 gchar * key, GtkBuilder * dialog)
{
	if (strcmp (key, GKBD_KEYBOARD_CONFIG_KEY_LAYOUTS) == 0) {
		xkb_layouts_fill_selected_tree (dialog);
		enable_disable_restoring (dialog);
	}
}

void
xkb_layouts_register_conf_listener (GtkBuilder * dialog)
{
	g_signal_connect (xkb_keyboard_settings, "changed",
			  G_CALLBACK (xkb_layouts_update_list), dialog);
}
