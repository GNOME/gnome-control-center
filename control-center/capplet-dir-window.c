/* -*- mode: c; style: linux -*- */

/* capplet-dir-window.c
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

#include "capplet-dir-window.h"

static void close_cb      (GtkWidget *widget, CappletDirWindow *window);
static void help_cb       (GtkWidget *widget, CappletDirWindow *window);
static void about_cb      (GtkWidget *widget, CappletDirWindow *window);

static void select_cb     (GtkWidget *widget, 
			   gint arg1, 
			   GdkEvent *event, 
			   CappletDirWindow *window);

static void about_done_cb (GtkWidget *widget, gpointer user_data);

static guint window_count;

static GtkWidget *about;

static GnomeUIInfo file_menu[] = {
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

CappletDirWindow *capplet_dir_window_new (CappletDir *dir) 
{
	CappletDirWindow *window;
	GtkWidget *swin;
	GtkAdjustment *adjustment;
	int i;

	swin = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);
	adjustment = gtk_scrolled_window_get_vadjustment
		(GTK_SCROLLED_WINDOW (swin));

	window = g_new0 (CappletDirWindow, 1);
	window->capplet_dir = dir;
	window->app =
		GNOME_APP (gnome_app_new ("control-center",
					  CAPPLET_DIR_ENTRY (dir)->label));
	window->icon_list =
		GNOME_ICON_LIST (gnome_icon_list_new (96, adjustment, 0));

	gtk_container_add (GTK_CONTAINER (swin), 
			   GTK_WIDGET (window->icon_list));

	gnome_icon_list_freeze (window->icon_list);

	for (i = 0; dir->entries[i]; i++)
		gnome_icon_list_insert (window->icon_list, i,
					dir->entries[i]->icon, 
					dir->entries[i]->label);

	gnome_icon_list_thaw (window->icon_list);

	gtk_widget_set_usize (GTK_WIDGET (window->app), 400, 300);

	gtk_signal_connect (GTK_OBJECT (window->app), "destroy",
			    GTK_SIGNAL_FUNC (close_cb), window);
	gtk_signal_connect (GTK_OBJECT (window->icon_list), "select-icon",
			    GTK_SIGNAL_FUNC (select_cb), window);

	gnome_app_create_menus_with_data (window->app, menu_bar, window);
	gnome_app_set_contents (window->app, swin);
	gtk_widget_show_all (GTK_WIDGET (window->app));

	window_count++;

	return window;
}

void capplet_dir_window_destroy (CappletDirWindow *window) 
{
	if (!window->destroyed) {
		window->destroyed = TRUE;

		if (!GTK_OBJECT_DESTROYED (GTK_OBJECT (window->app)))
			gtk_object_destroy (GTK_OBJECT (window->app));
		g_free (window);

		window_count--;

		if (window_count == 0) gtk_main_quit ();
	}
}

static void 
close_cb (GtkWidget *widget, CappletDirWindow *window)
{
	capplet_dir_window_destroy (window);
}

static void 
help_cb (GtkWidget *widget, CappletDirWindow *window)
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
about_cb (GtkWidget *widget, CappletDirWindow *window)
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
select_cb (GtkWidget *widget, gint arg1, GdkEvent *event, 
	   CappletDirWindow *window) 
{
	if (event->type == GDK_2BUTTON_PRESS &&
	    ((GdkEventButton *) event)->button == 1) 
	{
		capplet_dir_entry_activate
			(window->capplet_dir->entries[arg1]);
	}
}

static void
about_done_cb (GtkWidget *widget, gpointer user_data) 
{
	gtk_widget_hide (about);
}
