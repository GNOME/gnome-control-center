/* -*- mode: c; style: linux -*- */

/* capplet-dir-view.c
 * Copyright (C) 2000, 2001 Ximian, Inc.
 *
 * Written by Bradford Hovinen <hovinen@ximian.com>
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
#include <libgnomeui/gnome-window-icon.h>
#include <glade/glade.h>

#include "capplet-dir-view.h"

extern CappletDirViewImpl capplet_dir_view_list;
#if 0
extern CappletDirViewImpl capplet_dir_view_tree;
#endif

CappletDirViewImpl *capplet_dir_view_impl[] = {
	NULL,
	&capplet_dir_view_list,
#if 0
	&capplet_dir_view_tree,
#endif
};

static GObjectClass *parent_class;

enum {
	PROP_0,
	PROP_CAPPLET_DIR,
	PROP_LAYOUT
};

static GList *window_list;
static gboolean authed;

static void capplet_dir_view_init        (CappletDirView      *capplet_dir_view,
					  CappletDirViewClass *class);
static void capplet_dir_view_class_init  (CappletDirViewClass *class);

static void capplet_dir_view_set_prop    (GObject        *object, 
					  guint           prop_id,
					  const GValue   *value, 
					  GParamSpec     *pspec);
static void capplet_dir_view_get_prop    (GObject        *object,
					  guint           prop_id,
					  GValue         *value,
					  GParamSpec     *pspec);

static void capplet_dir_view_finalize    (GObject        *object);

static void close_cb                     (BonoboUIComponent *uic,
					  gpointer data,
					  const char *cname);
static void help_menu_cb		 (BonoboUIComponent *uic,
					  gpointer data,
					  const char *cname);
static void about_menu_cb                (BonoboUIComponent *uic,
					  gpointer data,
					  const char *cname);

GType
capplet_dir_view_get_type (void) 
{
	static GtkType capplet_dir_view_type = 0;

	if (!capplet_dir_view_type) {
		static const GTypeInfo capplet_dir_view_info = {
			sizeof (CappletDirViewClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) capplet_dir_view_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (CappletDirView),
			0 /* n_preallocs */,
			(GInstanceInitFunc) capplet_dir_view_init
		};

		capplet_dir_view_type = 
			g_type_register_static (G_TYPE_OBJECT,
						"CappletDirView",
						&capplet_dir_view_info,
						0);
	}

	return capplet_dir_view_type;
}

static BonoboUIVerb capplet_dir_view_verbs[] = {
	BONOBO_UI_VERB ("FileClose", close_cb),
	BONOBO_UI_VERB ("HelpContent", help_menu_cb),
	BONOBO_UI_VERB ("HelpAbout", about_menu_cb),
	BONOBO_UI_VERB_END
};

static void
capplet_dir_view_init (CappletDirView *view, CappletDirViewClass *class) 
{
	BonoboUIContainer *ui_container;
	BonoboUIComponent *ui_component;

	window_list = g_list_prepend (window_list, view);

	view->app = BONOBO_WINDOW (bonobo_window_new ("gnomecc", ""));
	ui_container = bonobo_window_get_ui_container (view->app);

	gtk_window_set_default_size (GTK_WINDOW (view->app), 620, 430);
	gnome_window_icon_set_from_file (GTK_WINDOW (view->app), 
					 PIXMAPS_DIR "/control-center.png");

	ui_component = bonobo_ui_component_new ("gnomecc");
	bonobo_ui_component_set_container (ui_component, bonobo_object_corba_objref (BONOBO_OBJECT (ui_container)), NULL);
	bonobo_ui_util_set_ui (ui_component,
		GNOMECC_DATA_DIR, "gnomecc-ui.xml", "gnomecc", NULL);

	g_signal_connect_swapped (G_OBJECT (view->app), "destroy",
				  (GCallback) g_object_unref, view);

	bonobo_ui_component_add_verb_list_with_data (ui_component, capplet_dir_view_verbs, view);
}

static void
capplet_dir_view_class_init (CappletDirViewClass *klass) 
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = capplet_dir_view_finalize;
	object_class->set_property = capplet_dir_view_set_prop;
	object_class->get_property = capplet_dir_view_get_prop;

	g_object_class_install_property
		(object_class, PROP_LAYOUT,
		 g_param_spec_int ("layout",
				   _("Layout"),
				   _("Layout to use for this view of the capplets"),
				   0, sizeof (capplet_dir_view_impl) / sizeof (CappletDirViewImpl *), 0,
				   G_PARAM_WRITABLE));
	g_object_class_install_property
		(object_class, PROP_CAPPLET_DIR,
		 g_param_spec_pointer ("capplet-dir",
				       _("Capplet directory object"),
				       _("Capplet directory that this view is viewing"),
				       G_PARAM_WRITABLE));

	parent_class = G_OBJECT_CLASS (g_type_class_ref (G_TYPE_OBJECT));
}

static void
capplet_dir_view_set_prop (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec) 
{
	CappletDirView *view;
	CappletDirViewLayout layout;

	view = CAPPLET_DIR_VIEW (object);

	switch (prop_id) {
	case PROP_CAPPLET_DIR:
		capplet_dir_view_load_dir (view, g_value_get_pointer (value));
		break;

	case PROP_LAYOUT:
#ifdef USE_HTML
		layout = CLAMP (g_value_get_int (value), 0, LAYOUT_HTML);
#else
		layout = CLAMP (g_value_get_int (value), 0, LAYOUT_ICON_LIST);
#endif

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

			bonobo_window_set_contents (view->app, view->view);

			if (view->capplet_dir && view->impl->populate)
				view->impl->populate (view);

			gtk_widget_show (view->view);
		}

		view->changing_layout = FALSE;
		break;

	default:
		g_warning ("Bad argument set");
		break;
	}
}

static void 
capplet_dir_view_get_prop (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec) 
{
	CappletDirView *view;

	view = CAPPLET_DIR_VIEW (object);

	switch (prop_id) {
	case PROP_CAPPLET_DIR:
		g_value_set_pointer (value, view->capplet_dir);
		break;

	case PROP_LAYOUT:
		g_value_set_uint (value, view->layout);
		break;

	default:
		g_warning ("Bad argument get");
		break;
	}
}

static void 
capplet_dir_view_finalize (GObject *object) 
{
	CappletDirView *view;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_CAPPLET_DIR_VIEW (object));

	view = CAPPLET_DIR_VIEW (object);

	view->capplet_dir->view = NULL;

	window_list = g_list_remove (window_list, view);

	if (g_list_length (window_list) == 0) 
		gtk_main_quit ();

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

void
capplet_dir_view_update_authenticated (CappletDirView *view, gpointer null)
{
}

CappletDirView *
capplet_dir_view_new (void) 
{
	GObject *view;

	view = g_object_new (capplet_dir_view_get_type (),
			     "layout", LAYOUT_ICON_LIST,
			     NULL);

	capplet_dir_view_update_authenticated
		(CAPPLET_DIR_VIEW (view), NULL);

	return CAPPLET_DIR_VIEW (view);
}

void
capplet_dir_views_set_authenticated (gboolean amiauthedornot)
{
	authed = amiauthedornot;
	g_list_foreach (window_list, (GFunc)capplet_dir_view_update_authenticated, NULL);
}

static void
close_cb (BonoboUIComponent *uic, gpointer data, const char *cname)
{
	CappletDirView *view = CAPPLET_DIR_VIEW (data);
	gtk_widget_destroy (GTK_WIDGET (CAPPLET_DIR_VIEW_W (view)));
}

static void
help_menu_cb (BonoboUIComponent *uic, gpointer data, const char *cname)
{
	GError *error = NULL;

	gnome_help_display_desktop (NULL,
		"control-center-manual",
		"control-center.xml",
		"intro", &error);
	if (error) {
		g_warning ("help error: %s\n", error->message);
		g_error_free (error);
	}
}

static void
about_menu_cb (BonoboUIComponent *uic, gpointer data, const char *cname)
{
	static GtkWidget *about = NULL;
	static gchar *authors[] = {
		"Jacob Berkman <jacob@ximian.com>",
		"Jonathan Blandford <jrb@redhat.com>",
		"Chema Celorio <chema@ximian.com>",
		"Rachel Hestilow <hestilow@ximian.com>",
		"Bradford Hovinen <hovinen@ximian.com>",
		"Lauris Kaplinski <lauris@ximian.com>",
		"Seth Nickell <snickell@stanford.edu>",
		"Jakub Steiner <jimmac@ximian.com>",
		NULL
	};

	static gchar *documenters[] = {
		NULL
	};

	gchar *translator_credits = _("translator_credits");

	if (about != NULL) {
		gdk_window_show (about->window);
		gdk_window_raise (about->window);
		return;
	}

	about = gnome_about_new
		(_("GNOME Control Center"), VERSION,
		 "Copyright (C) 2000, 2001 Ximian, Inc.\n"
		 "Copyright (C) 1999 Red Hat Software, Inc.",
		 _("Desktop properties manager."),
		 (const gchar **) authors,
		 (const gchar **) documenters,
		 strcmp (translator_credits, "translator_credits") != 0 ? translator_credits : NULL,
		 NULL);

	g_signal_connect (G_OBJECT (about), "destroy",
			  (GCallback) gtk_widget_destroyed, 
			  &about);

	gtk_widget_show (about);
}

#if 0

static void
menu_cb (GtkWidget *w, CappletDirView *view, CappletDirViewLayout layout)
{
	if (!GTK_CHECK_MENU_ITEM (w)->active || view->changing_layout)
		return;

	gtk_object_set (GTK_OBJECT (view), "layout", layout, NULL);
}

#endif

#ifdef USE_HTML
static void
html_menu_cb (GtkWidget *w, CappletDirView *view)
{
	menu_cb (w, view, LAYOUT_HTML);
}
#endif

#if 0

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

#endif

#ifdef USE_HTML
static void
html_toggle_cb (GtkWidget *w, CappletDirView *view)
{
	button_cb (w, view, LAYOUT_HTML);
}
#endif

#if 0

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

#if 0
static void
prefs_menu_cb (GtkWidget *widget, CappletDirView *view)
{
	gnomecc_preferences_get_config_dialog (prefs);
}
#endif

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

#endif

static void
option_menu_activate (GtkWidget *w, CappletDirEntry *entry)
{
	CappletDirView *view;

	view = g_object_get_data (G_OBJECT (w), "user_data");
	if (!IS_CAPPLET_DIR_VIEW (view))
		return;

	capplet_dir_entry_activate (entry, view);
}

void
capplet_dir_view_load_dir (CappletDirView *view, CappletDir *dir) 
{
	GtkWidget *menu, *menuitem, *w, *hbox;
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

	title = g_strdup_printf (_("Gnome Control Center : %s"), dir->entry.label);
	gtk_window_set_title (GTK_WINDOW (view->app), title);
	g_free (title);

	menu = gtk_menu_new ();

	for (entry = CAPPLET_DIR_ENTRY (dir); entry; entry = CAPPLET_DIR_ENTRY (entry->dir), parents++) {
		GdkPixbuf *pb, *pbs;

		menuitem = gtk_menu_item_new ();
		hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);

#if 0
		w = gnome_pixmap_new_from_file_at_size (entry->icon, 16, 16);
#else
		pb = gdk_pixbuf_new_from_file (entry->icon, NULL);
		pbs = gdk_pixbuf_scale_simple (pb, 16, 16, GDK_INTERP_HYPER);
		w = gtk_image_new_from_pixbuf (pb);
		g_object_unref (pbs);
		g_object_unref (pb);
#endif
		gtk_box_pack_start (GTK_BOX (hbox), w,
				    FALSE, FALSE, 0);

		w = gtk_label_new (entry->label);
		gtk_box_pack_start (GTK_BOX (hbox), w,
				    FALSE, FALSE, 0);

		gtk_container_add (GTK_CONTAINER (menuitem), hbox);

		if (entry != CAPPLET_DIR_ENTRY (dir)) {
			g_object_set_data (G_OBJECT (menuitem), "user_data", view);
			g_signal_connect (G_OBJECT (menuitem), "activate",
					  (GCallback) option_menu_activate,
					  entry);
		}
		
		gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menuitem);
	}
	gtk_widget_show_all (menu);
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
	if (launcher)
		return launcher;
	else
		return CAPPLET_DIR_VIEW (capplet_dir_view_new ());
}

void
gnomecc_init (void) 
{
	capplet_dir_init (get_capplet_dir_view);
}
