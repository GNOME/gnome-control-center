/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-mime-type-capplet.h
 *
 * Copyright (C) 1998 Redhat Software Inc. 
 * Copyright (C) 2000  Free Software Foundaton
 * Copyright (C) 2000  Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: 	Jonathan Blandford <jrb@redhat.com>
 * 		Gene Z. Ragan <gzr@eazel.com>
 */

#include <config.h>
#include <ctype.h>
#include <dirent.h>
#include <regex.h>
#include <string.h>
#include <sys/types.h>

#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>

#include "capplet-widget.h"
#include "gnome.h"
#include "mime-data.h"
#include "mime-info.h"
#include "edit-window.h"
#include "new-mime-window.h"

#include "nautilus-mime-type-capplet-dialogs.h"

#include "nautilus-mime-type-capplet.h"


/* Local Prototypes */
static void	init_mime_capplet 	  	(void);
static void 	populate_application_menu 	(GtkWidget 	*menu, 	 const char *mime_string);
static void 	populate_component_menu	  	(GtkWidget 	*menu, 	 const char *mime_string);
static void	delete_mime_clicked       	(GtkWidget 	*widget, gpointer   data);
static void	add_mime_clicked 	  	(GtkWidget 	*widget, gpointer   data);
static void	edit_applications_clicked 	(GtkWidget 	*widget, gpointer   data);
static void	edit_components_clicked   	(GtkWidget 	*widget, gpointer   data);

static void 	try_callback 		  	(void);
static void 	revert_callback 	  	(void);
static void 	ok_callback 		  	(void);
static void 	cancel_callback 	  	(void);
#if 0
static void 	help_callback 		 	(void);
#endif


GtkWidget *capplet = NULL;
GtkWidget *delete_button = NULL;
GtkWidget *icon_entry, *extension_list, *mime_list;
GtkWidget *application_menu, *component_menu;


/*
 *  main
 *
 *  Display capplet
 */

int
main (int argc, char **argv)
{
        int init_results;

        bindtextdomain (PACKAGE, GNOMELOCALEDIR);
        textdomain (PACKAGE);

        init_results = gnome_capplet_init("mime-type-capplet", VERSION,
                                          argc, argv, NULL, 0, NULL);

	if (init_results < 0) {
                exit (0);
	}

	if (init_results == 0) {
		init_mime_type ();
		init_mime_capplet ();
	        capplet_gtk_main ();
	}
        return 0;
}

static GtkWidget *
left_aligned_button (gchar *label)
{
  GtkWidget *button = gtk_button_new_with_label (label);
  gtk_misc_set_alignment (GTK_MISC (GTK_BIN (button)->child),
			  0.0, 0.5);
  gtk_misc_set_padding (GTK_MISC (GTK_BIN (button)->child),
			GNOME_PAD_SMALL, 0);
  
  return button;
}

static void
try_callback ()
{
        write_user_keys ();
        write_user_mime ();
}
static void
revert_callback ()
{
        write_initial_keys ();
        write_initial_mime ();
        discard_key_info ();
        discard_mime_info ();
        initialize_main_win_vals ();
}
static void
ok_callback ()
{
        write_user_keys ();
        write_user_mime ();
}
static void
cancel_callback ()
{
        write_initial_keys ();
        write_initial_mime ();
}

#if 0
static void
help_callback ()
{
        /* Sigh... empty as always */
}
#endif

static void
init_mime_capplet (void)
{
	GtkWidget *main_vbox;
        GtkWidget *vbox, *hbox;
        GtkWidget *button;
        GtkWidget *info_frame;
        GtkWidget *label;
        GtkWidget *fixed;
        GtkWidget *icon_entry;
        GtkWidget *mime_list_container;
        GList *children, *child;
	
        gchar *title[2] = {"Extensions"};
        
	capplet = capplet_widget_new ();

	/* Main vertical box */                    
	main_vbox = gtk_vbox_new (FALSE, GNOME_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (main_vbox), GNOME_PAD_SMALL);
        gtk_container_add (GTK_CONTAINER (capplet), main_vbox);

        /* Main horizontal box and mime list*/                    
        hbox = gtk_hbox_new (FALSE, GNOME_PAD);
        gtk_box_pack_start (GTK_BOX (main_vbox), hbox, TRUE, TRUE, 0);
        mime_list_container = create_mime_clist ();
        gtk_box_pack_start (GTK_BOX (hbox), mime_list_container, TRUE, TRUE, 0);         

	/* Get pointer to mime list */
	children = gtk_container_children (GTK_CONTAINER (mime_list_container));
	child = g_list_first (children);
	mime_list = child->data;

	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
      
        gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

	/* Set up info frame */	
        info_frame = gtk_frame_new ("Info");
        gtk_box_pack_end (GTK_BOX (main_vbox), info_frame, FALSE, FALSE, 0);

        fixed = gtk_fixed_new ();
	gtk_container_add (GTK_CONTAINER (info_frame), fixed);

 	/* Create label, menu and edit button for default applications */
	label = gtk_label_new (_("Default Application:"));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_fixed_put (GTK_FIXED (fixed), label, 10, 5);

	application_menu = gtk_option_menu_new();
	gtk_fixed_put (GTK_FIXED (fixed), application_menu, 125, 0);
	gtk_widget_set_usize (application_menu, 150, 25);

        button = left_aligned_button (_("Edit List"));
        gtk_fixed_put (GTK_FIXED (fixed), button, 285, 1);
	gtk_signal_connect (GTK_OBJECT (button), "clicked", edit_applications_clicked, mime_list);
	
	/* Create label, menu and edit button for default components */
	label = gtk_label_new (_("Default Component:"));
	gtk_fixed_put (GTK_FIXED (fixed), label, 10, 35);

	component_menu = gtk_option_menu_new();
	gtk_fixed_put (GTK_FIXED (fixed), component_menu, 125, 30);
	gtk_widget_set_usize (component_menu, 150, 25);

        button = left_aligned_button (_("Edit List"));
        gtk_fixed_put (GTK_FIXED (fixed), button, 285, 31);
	gtk_signal_connect (GTK_OBJECT (button), "clicked", edit_components_clicked, mime_list);

	/* Add icon box */
	icon_entry = gnome_icon_entry_new ("mime_icon_entry", _("Select an icon..."));
	//gtk_signal_connect (GTK_OBJECT (gnome_icon_entry_gtk_entry (GNOME_ICON_ENTRY (main_win->icon_entry))),
	//		    "changed",
	//		    entry_changed,
	//		    NULL);
	gtk_fixed_put (GTK_FIXED (fixed), icon_entry, 35, 65);

	/* Add extension list and buttons */
	extension_list = gtk_clist_new_with_titles (1, title);
	gtk_clist_column_titles_passive (GTK_CLIST (extension_list));
	gtk_clist_set_auto_sort (GTK_CLIST (extension_list), TRUE);

	//gtk_signal_connect (GTK_OBJECT (main_win->ext_clist), "select-row",
	//		    GTK_SIGNAL_FUNC (ext_clist_selected), NULL);
	//gtk_signal_connect (GTK_OBJECT (main_win->ext_clist), "unselect-row",
	//		    GTK_SIGNAL_FUNC (ext_clist_deselected), NULL);
	gtk_fixed_put (GTK_FIXED (fixed), extension_list, 125, 65);
	gtk_widget_set_usize (extension_list, 150, 60);

        button = gtk_button_new_with_label (_("Add"));
	gtk_fixed_put (GTK_FIXED (fixed), button, 285, 70);
	gtk_widget_set_usize (button, 60, 20);

        button = gtk_button_new_with_label (_("Remove"));
        gtk_fixed_put (GTK_FIXED (fixed), button, 285, 95);
	gtk_widget_set_usize (button, 60, 20);
	
	/* Mime list Add and Delete buttons */
        button = left_aligned_button (_("Add..."));
        gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
        gtk_signal_connect (GTK_OBJECT (button), "clicked", add_mime_clicked, NULL);
        
	delete_button = left_aligned_button (_("Delete"));
        gtk_signal_connect (GTK_OBJECT (delete_button), "clicked", delete_mime_clicked, NULL);
        gtk_box_pack_start (GTK_BOX (vbox), delete_button, FALSE, FALSE, 0);

        /* Yes, show all widgets */
        gtk_widget_show_all (capplet);

        /* Setup capplet signals */
        gtk_signal_connect(GTK_OBJECT(capplet), "try",
                           GTK_SIGNAL_FUNC(try_callback), NULL);
        gtk_signal_connect(GTK_OBJECT(capplet), "revert",
                           GTK_SIGNAL_FUNC(revert_callback), NULL);
        gtk_signal_connect(GTK_OBJECT(capplet), "ok",
                           GTK_SIGNAL_FUNC(ok_callback), NULL);
        gtk_signal_connect(GTK_OBJECT(capplet), "cancel",
                           GTK_SIGNAL_FUNC(cancel_callback), NULL);
        gtk_signal_connect(GTK_OBJECT(capplet), "page_hidden",
                           GTK_SIGNAL_FUNC(hide_edit_window), NULL);
        gtk_signal_connect(GTK_OBJECT(capplet), "page_shown",
                           GTK_SIGNAL_FUNC(show_edit_window), NULL);
#if 0
        gtk_signal_connect(GTK_OBJECT(capplet), "help",
                           GTK_SIGNAL_FUNC(help_callback), NULL);
#endif

	/* Force list to update menus */
	gtk_clist_select_row (GTK_CLIST (mime_list), 0, 0);

}


/*
 *  nautilus_mime_type_capplet_update_info
 *
 *  Update controls with info based on mime type	
 */
 
void
nautilus_mime_type_capplet_update_info (const char *mime_string) {
	populate_application_menu (application_menu, mime_string);
	populate_component_menu (component_menu, mime_string);
}

static void
populate_application_menu (GtkWidget *application_menu, const char *mime_type)
{
	GtkWidget *new_menu, *menu_item;
	GList *app_list, *copy_list;
	GnomeVFSMimeApplication *default_app, *application;
	gboolean has_none, found_match;
	char *mime_copy;
	const char *name;
	GList *children;
	int index;

	has_none = TRUE;
	found_match = FALSE;

	mime_copy = g_strdup (mime_type);
	
	new_menu = gtk_menu_new ();
	
	/* Get the default application */
	default_app = gnome_vfs_mime_get_default_application (mime_type);

	/* Get the application short list */
	app_list = gnome_vfs_mime_get_short_list_applications (mime_type);
	if (app_list != NULL) {
		for (copy_list = app_list; copy_list != NULL; copy_list = copy_list->next) {
			has_none = FALSE;

			application = copy_list->data;
			menu_item = gtk_menu_item_new_with_label (application->name);

			/* Store copy of application name in item; free when item destroyed. */
			gtk_object_set_data_full (GTK_OBJECT (menu_item),
						  "application",
						  g_strdup (application->name),
						  g_free);

			gtk_menu_append (GTK_MENU (new_menu), menu_item);
			gtk_widget_show (menu_item);
		}
	
		gnome_vfs_mime_application_list_free (app_list);
	}

	/* Find all appliactions or add a "None" item */
	if (has_none && default_app == NULL) {
		/* Add all applications */
		app_list = gnome_vfs_mime_get_all_applications (mime_type);
		for (copy_list = app_list; copy_list != NULL; copy_list = copy_list->next) {
			has_none = FALSE;

			application = copy_list->data;
			menu_item = gtk_menu_item_new_with_label (application->name);

			/* Store copy of application name in item; free when item destroyed. */
			gtk_object_set_data_full (GTK_OBJECT (menu_item),
						  "application",
						  g_strdup (application->name),
						  g_free);

			gtk_menu_append (GTK_MENU (new_menu), menu_item);
			gtk_widget_show (menu_item);
		}

		if (app_list != NULL) {
			gnome_vfs_mime_application_list_free (app_list);
			app_list = NULL;
		} else {
			menu_item = gtk_menu_item_new_with_label (_("None"));
			gtk_menu_append (GTK_MENU (new_menu), menu_item);
			gtk_widget_show (menu_item);
		}

	} else {
		/* Check and see if default is in the short list */
		if (default_app != NULL) {
			/* Iterate */
			children = gtk_container_children (GTK_CONTAINER (new_menu));
			for (index = 0; children != NULL; children = children->next, ++index) {				
				name = (const char *)(gtk_object_get_data (GTK_OBJECT (children->data), "application"));
				if (name != NULL) {											
					if (strcmp (default_app->name, name) == 0) {
						found_match = TRUE;
						break;
					}
				}
			}
			g_list_free (children);

			/* See if we have a match */
			if (found_match) {
				/* Have menu appear with default application selected */
				gtk_menu_set_active (GTK_MENU (new_menu), index);
			} else {
				/* FIXME bugzilla.eazel.com 1221: 
				 * What should we do in this case? 
				 * */
			}
		}
	}
	
	gtk_option_menu_set_menu (GTK_OPTION_MENU (application_menu), new_menu);
}


static void
populate_component_menu (GtkWidget *component_menu, const char *mime_string)
{
	GtkWidget *new_menu;
	GtkWidget *menu_item;
	GList *component_list;
	GList *list_element;
	gboolean has_none;
	char *component_name;

	has_none = TRUE;
	
	new_menu = gtk_menu_new ();

	/* Traverse the list looking for a match */
	component_list = gnome_vfs_mime_get_short_list_components (mime_string);
	for (list_element = component_list; list_element != NULL; list_element = list_element->next) {
		has_none = FALSE;

		component_name = name_from_oaf_server_info (list_element->data);
		menu_item = gtk_menu_item_new_with_label (component_name);
		gtk_menu_append (GTK_MENU (new_menu), menu_item);
		gtk_widget_show (menu_item);
		g_free (component_name);
	}

	if (mime_list != NULL) {
		gnome_vfs_mime_component_list_free (component_list);
	}
	
	/* Add None menu item */
	if (has_none) {
		menu_item = gtk_menu_item_new_with_label (_("None"));
		gtk_menu_append (GTK_MENU (new_menu), menu_item);
		gtk_widget_show (menu_item);
	}

	gtk_option_menu_set_menu (GTK_OPTION_MENU (component_menu), new_menu);
}

static void
free_mime_info (MimeInfo *mi)
{
}

/*
 *  delete_mime_clicked	
 *
 *  Delete mime type if it is a user defined type.
 */
 
static void
delete_mime_clicked (GtkWidget *widget, gpointer data)
{
        MimeInfo *mi;
        gint row = 0;

        if (GTK_CLIST (mime_list)->selection)
                row = GPOINTER_TO_INT ((GTK_CLIST (mime_list)->selection)->data);
        else
                return;
        mi = (MimeInfo *) gtk_clist_get_row_data (GTK_CLIST (mime_list), row);

        gtk_clist_remove (GTK_CLIST (mime_list), row);
        g_hash_table_remove (user_mime_types, mi->mime_type);
        remove_mime_info (mi->mime_type);
        free_mime_info (mi);
        capplet_widget_state_changed (CAPPLET_WIDGET (capplet),
                                      TRUE);
}

static void
add_mime_clicked (GtkWidget *widget, gpointer data)
{
	launch_new_mime_window ();
}

static void
edit_applications_clicked (GtkWidget *widget, gpointer data)
{	
	GtkWidget *list;
	MimeInfo *mi;
	GList *p;
	GtkCListRow *row;

	g_return_if_fail (GTK_IS_CLIST (data));

	list = data;
	row = NULL;

	/* Get first selected row.  This will be the only selection for this list */
	for (p = GTK_CLIST (list)->row_list; p != NULL; p = p->next) {
		row = p->data;
		if (row->state == GTK_STATE_SELECTED) {
			break;
		}
	}

	if (row == NULL) {
		return;
	}
	
	/* Show dialog */
	mi = (MimeInfo *) row->data;
	show_edit_applications_dialog (mi->mime_type);
}

static void
edit_components_clicked (GtkWidget *widget, gpointer data)
{
	GtkWidget *list;
	MimeInfo *mi;
	GList *p;
	GtkCListRow *row;

	g_return_if_fail (GTK_IS_CLIST (data));

	list = data;
	row = NULL;

	/* Get first selected row.  This will be the only selection for this list */
	for (p = GTK_CLIST (list)->row_list; p != NULL; p = p->next) {
		row = p->data;
		if (row->state == GTK_STATE_SELECTED) {
			break;
		}
	}

	if (row == NULL) {
		return;
	}
	
	/* Show dialog */
	mi = (MimeInfo *) row->data;
	show_edit_components_dialog (mi->mime_type);
}

