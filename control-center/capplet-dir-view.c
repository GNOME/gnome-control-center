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
#   include <config.h>
#endif

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glade/glade.h>

#include "capplet-dir-view.h"

extern CappletDirViewImpl capplet_dir_view_html;
extern CappletDirViewImpl capplet_dir_view_list;
extern CappletDirViewImpl capplet_dir_view_tree;

CappletDirViewImpl *capplet_dir_view_impl[] = {
	NULL,
	&capplet_dir_view_list,
	&capplet_dir_view_tree,
	&capplet_dir_view_html
};

static GtkObjectClass *parent_class;

static GnomeCCPreferences *prefs;

enum {
	ARG_0,
	ARG_CAPPLET_DIR,
	ARG_LAYOUT
};

static GList *window_list;
static gboolean authed;

static void
capplet_dir_view_update_authenticated (CappletDirView *view, gpointer null)
{
}

void
capplet_dir_views_set_authenticated (gboolean amiauthedornot)
{
	authed = amiauthedornot;
	g_list_foreach (window_list, (GFunc)capplet_dir_view_update_authenticated, NULL);
}

static void
capplet_dir_view_init (CappletDirView *view) 
{
	/* nothing to do here */
}

static void
capplet_dir_view_set_arg (GtkObject *object, GtkArg *arg, guint arg_id) 
{
	CappletDirView *view;
	CappletDirViewLayout layout;

	view = CAPPLET_DIR_VIEW (object);

	switch (arg_id) {
	case ARG_CAPPLET_DIR:
		capplet_dir_view_load_dir (view, GTK_VALUE_POINTER (*arg));
		break;
	case ARG_LAYOUT:
		layout = CLAMP (GTK_VALUE_UINT (*arg), 0, LAYOUT_HTML);
		if (layout == view->layout)
			break;

		g_assert (!view->changing_layout);
		view->changing_layout = TRUE;

		if (view->impl && view->impl->clean)
			view->impl->clean (view);

		view->layout =layout; 
		view->impl = capplet_dir_view_impl[layout];

		if (view->impl && view->impl->create) {
			view->view = view->impl->create (view);

			gnome_app_set_contents (view->app, view->view);

			if (view->capplet_dir && view->impl->populate)
				view->impl->populate (view);

#if 0			
			gtk_signal_connect (GTK_OBJECT (view->view), "destroy",
					    GTK_SIGNAL_FUNC (gtk_widget_destroyed),
					    &view->view);
#endif
			gtk_widget_show (view->view);
		}

		view->changing_layout = FALSE;
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

static void
capplet_dir_view_class_init (CappletDirViewClass *klass) 
{
	GtkObjectClass *object_class;

	parent_class = object_class = GTK_OBJECT_CLASS (klass);

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
}

guint
capplet_dir_view_get_type (void) 
{
	static guint capplet_dir_view_type = 0;

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
			gtk_type_unique (gtk_object_get_type (),
					 &capplet_dir_view_info);
	}

	return capplet_dir_view_type;
}

static void
print_somthing (GtkObject *o, char *s)
{
	g_print ("somthing destroyed: %s\n", s);
}

static void
destroy (GtkObject *o, GtkObject *o2)
{
	gtk_object_destroy (o2);
}

static void 
close_cb (GtkWidget *widget, CappletDirView *view)
{
	gtk_widget_destroy (GTK_WIDGET (CAPPLET_DIR_VIEW_W (view)));
}

static void
exit_cb (GtkWidget *w, gpointer data)
{
	gtk_main_quit ();
}

static void
menu_cb (GtkWidget *w, CappletDirView *view, CappletDirViewLayout layout)
{
	if (!GTK_CHECK_MENU_ITEM (w)->active || view->changing_layout)
		return;

	gtk_object_set (GTK_OBJECT (view), "layout", layout, NULL);
}

static void
html_menu_cb (GtkWidget *w, CappletDirView *view)
{
	menu_cb (w, view, LAYOUT_HTML);
}

static void
icon_menu_cb (GtkWidget *w, CappletDirView *view)
{
	menu_cb (w, view, LAYOUT_ICON_LIST);
}

static void
tree_menu_cb (GtkWidget *w, CappletDirView *view)
{
	menu_cb (w, view, LAYOUT_TREE);
}

static void
button_cb (GtkWidget *w, CappletDirView *view, CappletDirViewLayout layout)
{
	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (w)) || view->changing_layout)
		return;

	gtk_object_set (GTK_OBJECT (view), "layout", layout, NULL);
}

static void
html_toggle_cb (GtkWidget *w, CappletDirView *view)
{
	button_cb (w, view, LAYOUT_HTML);
}

static void
list_toggle_cb (GtkWidget *w, CappletDirView *view)
{
	button_cb (w, view, LAYOUT_ICON_LIST);
}

static void
tree_toggle_cb (GtkWidget *w, CappletDirView *view)
{
	button_cb (w, view, LAYOUT_TREE);
}

static void
prefs_menu_cb (GtkWidget *widget, CappletDirView *view)
{
	gnomecc_preferences_get_config_dialog (prefs);
}

static void
about_menu_cb (GtkWidget *widget, CappletDirView *view)
{
	static GtkWidget *about = NULL;
	static gchar *authors[] = {
		"Bradford Hovinen <hovinen@ximian.com>",
		"Jacob Berkman <jacob@ximian.com>",
		"Johnathan Blandford <jrb@redhat.com>",
		"Jakub Steiner <jimmac@ximian.com>",
		"Richard Hestilow <hestilow@ximian.com>",
		"Chema Celorio <chema@ximian.com>",
		NULL
	};

	if (about) {
		gdk_window_show (about->window);
		gdk_window_raise (about->window);
		return;
	}

	about = gnome_about_new
		(_("GNOME Control Center"), VERSION,
		 _("Desktop properties manager."),
		 (const gchar **) authors,
		 "Copyright (C) 2000, 2001 Ximian, Inc.\n"
		 "Copyright (C) 1999 Red Hat Software, Inc.",
		 NULL);

	gtk_signal_connect (GTK_OBJECT (about), "destroy",
			    GTK_SIGNAL_FUNC (gtk_widget_destroyed), 
			    &about);

	gtk_widget_show (about);
}

static void 
back_button_cb (GtkWidget *widget, CappletDirView *view)
{
	if (CAPPLET_DIR_ENTRY (view->capplet_dir)->dir)
		capplet_dir_view_load_dir
			(view, CAPPLET_DIR_ENTRY (view->capplet_dir)->dir);
}

static void
rootm_button_cb (GtkWidget *w, CappletDirView *view)
{
	gdk_beep ();
}

CappletDirView *
capplet_dir_view_new (void) 
{
	GladeXML *xml;
	CappletDirView *view;

	xml = glade_xml_new (GLADEDIR"/gnomecc.glade", "main_window");
	if (!xml)
		return NULL;


	view = CAPPLET_DIR_VIEW (gtk_type_new (CAPPLET_DIR_VIEW_TYPE));

	window_list = g_list_append (window_list, view);

	view->app = GNOME_APP (glade_xml_get_widget (xml, "main_window"));

	gtk_signal_connect (GTK_OBJECT (view->app), "destroy",
			    GTK_SIGNAL_FUNC (destroy), view);

	glade_xml_signal_connect_data (xml, "close_cb", close_cb, view);

	glade_xml_signal_connect_data (xml, "about_menu_cb", about_menu_cb, view);
	gtk_object_unref (GTK_OBJECT (xml));
	
	gtk_object_set (GTK_OBJECT (view), "layout", prefs->layout, NULL);

	capplet_dir_view_update_authenticated (view, NULL);

	return view;
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

	/* GTK_OBJECT_CLASS (parent_class)->destroy (GTK_OBJECT (view)); */
}

static void
option_menu_activate (GtkWidget *w, CappletDirEntry *entry)
{
	CappletDirView *view;

	view = gtk_object_get_user_data (GTK_OBJECT (w));
	if (!IS_CAPPLET_DIR_VIEW (view))
		return;

	capplet_dir_entry_activate (entry, view);
}

void
capplet_dir_view_load_dir (CappletDirView *view, CappletDir *dir) 
{
	GtkWidget *menu, *menuitem, *w, *hbox;
	GdkPixbuf *pb, *scaled;
	GdkPixmap *pixmap;
	GdkBitmap *bitmap;
	CappletDirEntry *entry;
	int parents = 0;
	gchar *title;

	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_CAPPLET_DIR_VIEW (view));

	view->capplet_dir = dir;

	if (view->impl && view->impl->clear)
		view->impl->clear (view);

	if (!dir || view->layout == LAYOUT_NONE) return;

	if (view->impl && view->impl->populate)
		view->impl->populate (view);

	title = g_strdup_printf (_("Gnome Control Center : %s"), dir->entry.entry->name);
	gtk_window_set_title (GTK_WINDOW (view->app), title);
	g_free (title);

	menu = gtk_menu_new ();

	for (entry = CAPPLET_DIR_ENTRY (dir); entry; entry = CAPPLET_DIR_ENTRY (entry->dir), parents++) {
		menuitem = gtk_menu_item_new ();
		hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);

		w = gnome_pixmap_new_from_file_at_size (entry->icon, 16, 16);
		gtk_box_pack_start (GTK_BOX (hbox), w,
				    FALSE, FALSE, 0);

		w = gtk_label_new (entry->label);
		gtk_box_pack_start (GTK_BOX (hbox), w,
				    FALSE, FALSE, 0);

		gtk_container_add (GTK_CONTAINER (menuitem), hbox);

		if (entry != CAPPLET_DIR_ENTRY (dir)) {
			gtk_object_set_user_data (GTK_OBJECT (menuitem), view);
			gtk_signal_connect (GTK_OBJECT (menuitem), "activate",
					    GTK_SIGNAL_FUNC (option_menu_activate),
					    entry);
		}
		
		gtk_menu_prepend (GTK_MENU (menu), menuitem);
	}
	gtk_widget_show_all (menu);
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


#if 0
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
#endif

static void
prefs_changed_cb (GnomeCCPreferences *prefs) 
{
	GList *node;
	CappletDirView *view;

	for (node = window_list; node; node = node->next)
		gtk_object_set (GTK_OBJECT (node->data), "layout", prefs->layout, NULL);
}

void
capplet_dir_view_show (CappletDirView *view)
{
	g_return_if_fail (view != NULL);
	g_return_if_fail (IS_CAPPLET_DIR_VIEW (view));

	gtk_widget_show (GTK_WIDGET (CAPPLET_DIR_VIEW_W (view)));
}

static CappletDirView *
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
