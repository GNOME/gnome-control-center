/* -*- mode: c; style: linux -*- */

/* capplet-dir-view.c
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen (hovinen@helixcode.com)
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

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "capplet-dir-view.h"

static GnomeAppClass *parent_class;
static GtkCTreeClass *ctree_class;

static GnomeCCPreferences *prefs;

enum {
	ARG_0,
	ARG_CAPPLET_DIR,
	ARG_LAYOUT
};

static GList *window_list;

static GtkWidget *about;

static void capplet_dir_view_init (CappletDirView *view);
static void capplet_dir_view_class_init (CappletDirViewClass *klass);

static void capplet_dir_view_set_arg (GtkObject *object,
				      GtkArg *arg,
				      guint arg_id);

static void capplet_dir_view_get_arg (GtkObject *object,
				      GtkArg *arg,
				      guint arg_id);

static gint tree_event_cb        (GtkCTree *ctree,
				  GdkEventButton *event,
				  CappletDirView *view);

static void clear                (CappletDirView *view);
static void clean                (CappletDirView *view);

static void switch_to_icon_list  (CappletDirView *view);
static void create_icon_list     (CappletDirView *view);
static void populate_icon_list   (CappletDirView *view);

static void switch_to_tree       (CappletDirView *view);
static void create_tree          (CappletDirView *view);
static void populate_tree_branch (CappletDir *dir, GtkCTree *ctree, 
				  GtkCTreeNode *parent);
static void populate_tree        (CappletDirView *view);

static void select_icon_list_cb  (GtkWidget *widget, 
				  gint arg1, GdkEvent *event, 
				  CappletDirView *view);
static void select_tree_cb       (GtkCTree *ctree, 
				  GtkCTreeNode *node, gint column,
				  GdkEventButton *event, 
				  CappletDirView *view);

static void preferences_cb       (GtkWidget *widget, CappletDirView *view);
static void close_cb             (GtkWidget *widget, CappletDirView *view);
static void help_cb              (GtkWidget *widget, CappletDirView *view);
static void about_cb             (GtkWidget *widget, CappletDirView *view);

static void up_cb                (GtkWidget *widget, CappletDirView *view);
static void icons_cb             (GtkWidget *widget, CappletDirView *view);
static void tree_cb              (GtkWidget *widget, CappletDirView *view);

static void prefs_changed_cb     (GnomeCCPreferences *prefs);

static void about_done_cb        (GtkWidget *widget, gpointer user_data);

CappletDirView *get_capplet_dir_view (CappletDir *dir, 
				      CappletDirView *launcher);

static GnomeUIInfo file_menu[] = {
	GNOMEUIINFO_MENU_PREFERENCES_ITEM (preferences_cb, NULL),
        GNOMEUIINFO_MENU_CLOSE_ITEM (close_cb, NULL), 
        GNOMEUIINFO_END
};

static GnomeUIInfo help_menu[] = {

        GNOMEUIINFO_ITEM_STOCK (N_("Help on control-center"), 
				N_("Help with the GNOME control-center."),
				help_cb, GNOME_STOCK_PIXMAP_HELP),
        GNOMEUIINFO_SEPARATOR,
        GNOMEUIINFO_ITEM_STOCK (N_("About"), 
				N_("About the GNOME control-center."),
				about_cb, GNOME_STOCK_MENU_ABOUT),
        GNOMEUIINFO_END
};

static GnomeUIInfo menu_bar[] = {
        GNOMEUIINFO_MENU_FILE_TREE (file_menu),
        GNOMEUIINFO_MENU_HELP_TREE (help_menu),
	GNOMEUIINFO_END
};

static GnomeUIInfo tool_bar[] = {
	GNOMEUIINFO_ITEM_STOCK (N_("Up"), N_("Parent Group"), up_cb,
				GNOME_STOCK_PIXMAP_UP),
	GNOMEUIINFO_ITEM_STOCK (N_("Preferences"), 
				N_("Control Center Preferences"), 
				preferences_cb, 
				GNOME_STOCK_PIXMAP_PREFERENCES),
	GNOMEUIINFO_ITEM_STOCK (N_("Close"), N_("Close this Window"),
				close_cb, GNOME_STOCK_PIXMAP_CLOSE),
	GNOMEUIINFO_END
};

guint
capplet_dir_view_get_type (void) 
{
	static guint capplet_dir_view_type;

	if (!capplet_dir_view_type) {
		GtkTypeInfo capplet_dir_view_info = {
			"CappletDirView",
			sizeof (CappletDirView),
			sizeof (CappletDirViewClass),
			(GtkClassInitFunc) capplet_dir_view_class_init,
			(GtkObjectInitFunc) capplet_dir_view_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		capplet_dir_view_type = 
			gtk_type_unique (gnome_app_get_type (),
					 &capplet_dir_view_info);
	}

	return capplet_dir_view_type;
}

static void
capplet_dir_view_init (CappletDirView *view) 
{
	view->layout = LAYOUT_NONE;

	gnome_app_construct (GNOME_APP (view),
			     "control-center",
			     _("Control Center"));

	gtk_widget_set_usize (GTK_WIDGET (view), 400, 300);

	view->scrolled_win = GTK_SCROLLED_WINDOW
		(gtk_scrolled_window_new (NULL, NULL));

	gtk_scrolled_window_set_policy (view->scrolled_win,
					GTK_POLICY_NEVER,
					GTK_POLICY_ALWAYS);

	gnome_app_create_menus_with_data (GNOME_APP (view), menu_bar, view);
	gnome_app_create_toolbar_with_data (GNOME_APP (view), tool_bar, view);
	gnome_app_set_contents (GNOME_APP (view), 
				GTK_WIDGET (view->scrolled_win));

	view->up_button = tool_bar[0].widget;
}

static void
capplet_dir_view_class_init (CappletDirViewClass *klass) 
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);

	object_class->destroy = 
		(void (*) (GtkObject *)) capplet_dir_view_destroy;
	object_class->set_arg = capplet_dir_view_set_arg;
	object_class->get_arg = capplet_dir_view_get_arg;

	gtk_object_add_arg_type ("CappletDirView::layout",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_LAYOUT);

	gtk_object_add_arg_type ("CappletDirView::capplet_dir",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_CAPPLET_DIR);

	parent_class = gtk_type_class (gnome_app_get_type ());
	ctree_class = gtk_type_class (gtk_ctree_get_type ());
}

static void
capplet_dir_view_set_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	CappletDirView *view;

	view = CAPPLET_DIR_VIEW (object);

	switch (arg_id) {
	case ARG_CAPPLET_DIR:
		capplet_dir_view_load_dir (view, GTK_VALUE_POINTER (*arg));
		break;
	case ARG_LAYOUT:
		switch (GTK_VALUE_UINT (*arg)) {
		case LAYOUT_ICON_LIST: switch_to_icon_list (view); break;
		case LAYOUT_TREE: switch_to_tree (view); break;
		}
		break;
	default:
		break;
	}
}

static void 
capplet_dir_view_get_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	CappletDirView *view;

	view = CAPPLET_DIR_VIEW (object);

	switch (arg_id) {
	case ARG_CAPPLET_DIR:
		GTK_VALUE_POINTER (*arg) = view->capplet_dir;
		break;
	case ARG_LAYOUT:
		GTK_VALUE_UINT (*arg) = view->layout;
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

GtkWidget *
capplet_dir_view_new (void) 
{
	GtkWidget *widget;

	widget = gtk_widget_new (capplet_dir_view_get_type (),
				 "layout", prefs->layout,
				 NULL);

	window_list = g_list_append (window_list, widget);

	return widget;
}

void 
capplet_dir_view_destroy (CappletDirView *view) 
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_CAPPLET_DIR_VIEW (view));

	view->capplet_dir->view = NULL;

	window_list = g_list_remove (window_list, view);

	if (g_list_length (window_list) == 0) 
		gtk_main_quit ();

	GTK_OBJECT_CLASS (parent_class)->destroy (GTK_OBJECT (view));
}

void
capplet_dir_view_load_dir (CappletDirView *view, CappletDir *dir) 
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_CAPPLET_DIR_VIEW (view));

	view->capplet_dir = dir;

	clear (view);

	if (!dir || view->layout == LAYOUT_NONE) return;

	switch (view->layout) {
	case LAYOUT_ICON_LIST: populate_icon_list (view); break;
	case LAYOUT_TREE:      populate_tree (view);      break;
	}

	if (CAPPLET_DIR_ENTRY (dir)->dir == NULL)
		gtk_widget_set_sensitive (view->up_button, FALSE);
	else
		gtk_widget_set_sensitive (view->up_button, TRUE);
}

/* Clear all the icons/entries from the view */

static void
clear (CappletDirView *view) 
{
	switch (view->layout) {
	case LAYOUT_ICON_LIST: 
		gnome_icon_list_clear (view->u.icon_list);
		break;
	case LAYOUT_TREE:
		gtk_clist_clear (GTK_CLIST (view->u.tree));
		break;
	}
}

/* Destroy the object that holds the view */

static void
clean (CappletDirView *view) 
{
	switch (view->layout) {
	case LAYOUT_ICON_LIST:
		gtk_object_destroy (GTK_OBJECT (view->u.icon_list));
		break;
	case LAYOUT_TREE:
		gtk_object_destroy (GTK_OBJECT (view->u.tree));
		break;
	}
}

static void
switch_to_icon_list (CappletDirView *view) 
{
	if (view->layout != LAYOUT_ICON_LIST) {
		clean (view);
		create_icon_list (view);
		view->layout = LAYOUT_ICON_LIST;
	}
}

static void
create_icon_list (CappletDirView *view) 
{
	GtkAdjustment *adjustment;
	int i;

	adjustment = gtk_scrolled_window_get_vadjustment
		(GTK_SCROLLED_WINDOW (view->scrolled_win));

	view->u.icon_list =
		GNOME_ICON_LIST (gnome_icon_list_new (96, adjustment, 0));
	gtk_container_add (GTK_CONTAINER (view->scrolled_win), 
			   GTK_WIDGET (view->u.icon_list));

	if (view->selected)
		view->capplet_dir = view->selected->dir;

	if (view->capplet_dir) populate_icon_list (view);

	if (view->selected) {
		for (i = 0; view->capplet_dir->entries[i]; i++)
			if (view->capplet_dir->entries[i] == view->selected)
				break;
		if (view->capplet_dir->entries[i])
			gnome_icon_list_select_icon (view->u.icon_list, i);
	}

	gtk_signal_connect (GTK_OBJECT (view->u.icon_list), 
			    "select-icon", 
			    GTK_SIGNAL_FUNC (select_icon_list_cb),
			    view);
	gtk_widget_show (GTK_WIDGET (view->u.icon_list));
}

static void 
populate_icon_list (CappletDirView *view) 
{
	int i;

	gnome_icon_list_freeze (view->u.icon_list);

	for (i = 0; view->capplet_dir->entries[i]; i++)
		gnome_icon_list_insert 
			(view->u.icon_list, i,
			 view->capplet_dir->entries[i]->icon, 
			 view->capplet_dir->entries[i]->label);

	gnome_icon_list_thaw (view->u.icon_list);
}

static void
switch_to_tree (CappletDirView *view) 
{
	CappletDir *dir, *old_dir;
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

static void
create_tree (CappletDirView *view) 
{
	GtkAdjustment *adjustment;

	adjustment = gtk_scrolled_window_get_vadjustment
		(GTK_SCROLLED_WINDOW (view->scrolled_win));

	view->u.tree = GTK_CTREE (gtk_ctree_new (1, 0));
	gtk_container_add (GTK_CONTAINER (view->scrolled_win), 
			   GTK_WIDGET (view->u.tree));
	if (view->capplet_dir) populate_tree (view);
	gtk_signal_connect (GTK_OBJECT (view->u.tree), 
			    "tree-select-row", 
			    GTK_SIGNAL_FUNC (select_tree_cb),
			    view);
	gtk_signal_connect (GTK_OBJECT (view->u.tree), 
			    "button_press_event", 
			    GTK_SIGNAL_FUNC (tree_event_cb),
			    view);
	gtk_widget_show (GTK_WIDGET (view->u.tree));
}

static void
populate_tree_branch (CappletDir *dir, GtkCTree *ctree, GtkCTreeNode *parent) 
{
	GdkPixbuf *pixbuf, *scaled;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	GtkCTreeNode *current = NULL;
	int i;

	capplet_dir_load (dir);

	for (i = 0; dir->entries[i]; i++) {
		pixbuf = gdk_pixbuf_new_from_file (dir->entries[i]->icon);
		scaled = gdk_pixbuf_scale_simple (pixbuf, 16, 16, 
						  GDK_INTERP_TILES);
		gdk_pixbuf_render_pixmap_and_mask (scaled, &pixmap, &mask, 1);
		gdk_pixbuf_unref (pixbuf);
		gdk_pixbuf_unref (scaled);

		current = gtk_ctree_insert_node 
			(ctree, parent, NULL,
			 &(dir->entries[i]->label),
			 10, pixmap, mask, pixmap, mask,
			 IS_CAPPLET (dir->entries[i]), FALSE);
		gtk_ctree_node_set_row_data (ctree, current, dir->entries[i]);

		if (IS_CAPPLET_DIR (dir->entries[i]))
			populate_tree_branch (CAPPLET_DIR (dir->entries[i]), 
					      ctree, current);
	}
}

static void 
populate_tree (CappletDirView *view) 
{
	gtk_clist_freeze (GTK_CLIST (view->u.tree));
	populate_tree_branch (view->capplet_dir, view->u.tree, NULL);
	gtk_clist_thaw (GTK_CLIST (view->u.tree));
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
}

static void 
select_icon_list_cb (GtkWidget *widget, gint arg1, GdkEvent *event, 
		     CappletDirView *view) 
{
	if (event->type == GDK_2BUTTON_PRESS &&
	    ((GdkEventButton *) event)->button == 1) 
	{
		capplet_dir_entry_activate
			(view->capplet_dir->entries[arg1], view);
		view->selected = NULL;
	} else {
		view->selected = view->capplet_dir->entries[arg1];
	}
}

static void
select_tree_cb (GtkCTree *ctree, GtkCTreeNode *node, gint column,
		GdkEventButton *event, CappletDirView *view) 
{
	CappletDirEntry *dir_entry;

	dir_entry = gtk_ctree_node_get_row_data (ctree, node);
	view->selected = dir_entry;
}

static void
preferences_cb (GtkWidget *widget, CappletDirView *view)
{
	gnomecc_preferences_get_config_dialog (prefs);
}

static void 
close_cb (GtkWidget *widget, CappletDirView *view)
{
	gtk_object_destroy (GTK_OBJECT (view));
}

static void 
help_cb (GtkWidget *widget, CappletDirView *view)
{
	gchar *tmp;

	tmp = gnome_help_file_find_file ("users-guide", "gcc.html");

	if (tmp) {
		gnome_help_goto (0, tmp);
		g_free (tmp);
	} else {
		GtkWidget *mbox;

		mbox = gnome_message_box_new
			(_("No help is available/installed. Please " \
			   "make sure you\nhave the GNOME User's " \
			   "Guide installed on your system."),
			 GNOME_MESSAGE_BOX_ERROR, _("Close"), NULL);

		gtk_widget_show (mbox);
	}
}

static void
about_cb (GtkWidget *widget, CappletDirView *view)
{
	static gchar *authors[] = {
		"Bradford Hovinen <hovinen@helixcode.com>",
		NULL
	};

	if (about == NULL) {
		about = gnome_about_new
			(_("GNOME Control Center"), VERSION,
			 "Copyright (C) 2000 Helix Code, Inc.\n",
			 (const gchar **) authors,
			 _("Desktop Properties manager."),
			 NULL);

		gtk_signal_connect (GTK_OBJECT (about), "destroy", 
				    about_done_cb, NULL);
        }

	gtk_widget_show (about);
}

static void
about_done_cb (GtkWidget *widget, gpointer user_data) 
{
	gtk_widget_hide (about);
}

static void 
up_cb (GtkWidget *widget, CappletDirView *view)
{
	if (CAPPLET_DIR_ENTRY (view->capplet_dir)->dir)
		capplet_dir_view_load_dir
			(view, CAPPLET_DIR_ENTRY (view->capplet_dir)->dir);
}

static void 
icons_cb (GtkWidget *widget, CappletDirView *view)
{
	switch_to_icon_list (view);
}

static void 
tree_cb (GtkWidget *widget, CappletDirView *view)
{
	switch_to_tree (view);
}

static void
prefs_changed_cb (GnomeCCPreferences *prefs) 
{
	GList *node;
	CappletDirView *view;

	switch (prefs->layout) {
	case LAYOUT_ICON_LIST:
		for (node = window_list; node; node = node->next)
			switch_to_icon_list (CAPPLET_DIR_VIEW (node->data));
		break;
	case LAYOUT_TREE:
		node = window_list;

		switch_to_tree (CAPPLET_DIR_VIEW (node->data));
		capplet_dir_view_load_dir 
			(CAPPLET_DIR_VIEW (node->data),
			 get_root_capplet_dir ());
		node = node->next;

		while (node) {
			view = CAPPLET_DIR_VIEW (node->data);
			node = node->next;
			gtk_object_destroy (GTK_OBJECT (view));
		}

		break;
	}
}

CappletDirView *
get_capplet_dir_view (CappletDir *dir, CappletDirView *launcher) 
{
	if (prefs->single_window && launcher)
		return launcher;
	else
		return CAPPLET_DIR_VIEW (capplet_dir_view_new ());
}

void
gnomecc_init (void) 
{
	prefs = gnomecc_preferences_new ();
	gnomecc_preferences_load (prefs);

	gtk_signal_connect (GTK_OBJECT (prefs), "changed",
			    prefs_changed_cb, NULL);

	capplet_dir_init (get_capplet_dir_view);
}
