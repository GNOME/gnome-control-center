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
	SEL_LAYOUT_TREE_COL_ENABLED,
	SEL_LAYOUT_N_COLS
};

static int idx2select = -1;
static int max_selected_layouts = -1;

static GtkCellRenderer *text_renderer;

static gboolean disable_buttons_sensibility_update = FALSE;

static gboolean
get_selected_iter (GtkBuilder    *dialog,
		   GtkTreeModel **model,
		   GtkTreeIter   *iter)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (WID ("xkb_layouts_selected")));

	return gtk_tree_selection_get_selected (selection, model, iter);
}

static void
set_selected_path (GtkBuilder    *dialog,
		   GtkTreePath   *path)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (WID ("xkb_layouts_selected")));

	gtk_tree_selection_select_path (selection, path);
}

static gint
find_selected_layout_idx (GtkBuilder *dialog)
{
	GtkTreeIter selected_iter;
	GtkTreeModel *model;
	GtkTreePath *path;
	gint *indices;
	gint rv;

	if (!get_selected_iter (dialog, &model, &selected_iter))
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
update_layouts_list (GtkTreeModel *model,
		     GtkBuilder   *dialog)
{
	gboolean cont;
	GtkTreeIter iter;
	GPtrArray *array;

	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);
	cont = gtk_tree_model_get_iter_first (model, &iter);
	while (cont) {
		char *id;

		gtk_tree_model_get (model, &iter,
				    SEL_LAYOUT_TREE_COL_ID, &id,
				    -1);
		g_ptr_array_add (array, id);
		cont = gtk_tree_model_iter_next (model, &iter);
	}
	g_ptr_array_add (array, NULL);
	xkb_layouts_set_selected_list (array->pdata);
	g_ptr_array_free (array, TRUE);

	xkb_layouts_enable_disable_buttons (dialog);
}

static void
xkb_layouts_drag_end (GtkWidget	     *widget,
		      GdkDragContext *drag_context,
		      gpointer	      user_data)
{
	update_layouts_list (gtk_tree_view_get_model (GTK_TREE_VIEW (widget)),
			     GTK_BUILDER (user_data));
}

void
xkb_layouts_prepare_selected_tree (GtkBuilder * dialog)
{
	GtkListStore *list_store;
	GtkWidget *tree_view = WID ("xkb_layouts_selected");
	GtkTreeSelection *selection;
	GtkTreeViewColumn *desc_column;

	list_store = gtk_list_store_new (SEL_LAYOUT_N_COLS,
					 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);

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
	gtk_tree_view_set_reorderable (GTK_TREE_VIEW (tree_view), TRUE);
	g_signal_connect (G_OBJECT (tree_view), "drag-end",
			  G_CALLBACK (xkb_layouts_drag_end), dialog);
}

gchar *
xkb_layout_description_utf8 (const gchar * visible)
{
	char *l, *sl, *v, *sv;
	if (gkbd_keyboard_config_get_descriptions
	    (config_registry, visible, &sl, &l, &sv, &v))
		visible =
		    gkbd_keyboard_config_format_full_description (l, v);
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
		char *cur_layout = layouts[i];
		gchar *utf_visible =
		    xkb_layout_description_utf8 (cur_layout);

		gtk_list_store_insert_with_values (list_store, NULL, G_MAXINT,
						   SEL_LAYOUT_TREE_COL_DESCRIPTION,
						   utf_visible,
						   SEL_LAYOUT_TREE_COL_ID,
						   cur_layout,
						   SEL_LAYOUT_TREE_COL_ENABLED,
						   i < max_selected_layouts, -1);
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
add_default_switcher_if_necessary ()
{
	gchar **layouts_list = xkb_layouts_get_selected_list();
	gchar **options_list = xkb_options_get_selected_list ();
	gboolean was_appended;

	options_list =
	    gkbd_keyboard_config_add_default_switch_option_if_necessary
	    (layouts_list, options_list, &was_appended);
	if (was_appended)
		xkb_options_set_selected_list (options_list);
	g_strfreev (options_list);
}

static void
chooser_response (GtkDialog  *chooser,
		  int         response_id,
		  GtkBuilder *dialog)
{
	if (response_id == GTK_RESPONSE_OK) {
		char *id, *name;
		GtkListStore *list_store;

		list_store = GTK_LIST_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (WID ("xkb_layouts_selected"))));
		id = xkb_layout_chooser_get_selected_id (chooser);
		name = xkb_layout_description_utf8 (id);
		gtk_list_store_insert_with_values (list_store, NULL, G_MAXINT,
						   SEL_LAYOUT_TREE_COL_DESCRIPTION, name,
						   SEL_LAYOUT_TREE_COL_ID, id,
						   SEL_LAYOUT_TREE_COL_ENABLED, TRUE,
						   -1);
		g_free (name);
		add_default_switcher_if_necessary ();
		update_layouts_list (GTK_TREE_MODEL (list_store), dialog);
	}

	xkb_layout_chooser_response (chooser, response_id);
}

static void
add_selected_layout (GtkWidget * button, GtkBuilder * dialog)
{
	GtkWidget *chooser;

	chooser = xkb_layout_choose (dialog);
	g_signal_connect (G_OBJECT (chooser), "response",
			  G_CALLBACK (chooser_response), dialog);
}

static void
show_selected_layout (GtkWidget * button, GtkBuilder * dialog)
{
	gint idx = find_selected_layout_idx (dialog);

	if (idx != -1) {
		GtkWidget *parent = WID ("region_notebook");
		GtkWidget *popup = gkbd_keyboard_drawing_dialog_new ();
		gkbd_keyboard_drawing_dialog_set_group (popup,
							config_registry,
							idx);
		gtk_window_set_transient_for (GTK_WINDOW (popup),
					      GTK_WINDOW
					      (gtk_widget_get_toplevel
					       (parent)));
		gtk_widget_show_all (popup);
	}
}

static void
remove_selected_layout (GtkWidget * button, GtkBuilder * dialog)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	if (get_selected_iter (dialog, &model, &iter) == FALSE)
		return;

	gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
	update_layouts_list (model, dialog);
}

static void
move_up_selected_layout (GtkWidget * button, GtkBuilder * dialog)
{
	GtkTreeModel *model;
	GtkTreeIter iter, prev;
	GtkTreePath *path;

	if (get_selected_iter (dialog, &model, &iter) == FALSE)
		return;

	prev = iter;
	if (!gtk_tree_model_iter_previous (model, &prev))
		return;

	path = gtk_tree_model_get_path (model, &prev);
	
	gtk_list_store_swap (GTK_LIST_STORE (model), &iter, &prev);

	update_layouts_list (model, dialog);
	
	set_selected_path (dialog, path);

	gtk_tree_path_free (path);	
}

static void
move_down_selected_layout (GtkWidget * button, GtkBuilder * dialog)
{
	GtkTreeModel *model;
	GtkTreeIter iter, next;
	GtkTreePath *path;

	if (get_selected_iter (dialog, &model, &iter) == FALSE)
		return;

	next = iter;
	if (!gtk_tree_model_iter_next (model, &next))
		return;

	path = gtk_tree_model_get_path (model, &next);

	gtk_list_store_swap (GTK_LIST_STORE (model), &iter, &next);

	update_layouts_list (model, dialog);

	set_selected_path (dialog, path);

	gtk_tree_path_free (path);	
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
