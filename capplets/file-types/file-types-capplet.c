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

#include <capplet-widget.h>
#include <gnome.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>

#include "nautilus-mime-type-capplet-dialogs.h"

#include "nautilus-mime-type-capplet.h"


/* Local Prototypes */
static void	 init_mime_capplet 	  	(void);
static void 	 populate_application_menu 	(GtkWidget 	*menu, 	 const char *mime_string);
static void 	 populate_component_menu	(GtkWidget 	*menu, 	 const char *mime_string);
static void	 delete_mime_clicked       	(GtkWidget 	*widget, gpointer   data);
static void	 add_mime_clicked 	  	(GtkWidget 	*widget, gpointer   data);
static void	 edit_applications_clicked 	(GtkWidget 	*widget, gpointer   data);
static void	 edit_components_clicked   	(GtkWidget 	*widget, gpointer   data);
static GtkWidget *create_mime_list_and_scroller (void);


static void 	ok_callback 		  	(void);

GtkWidget *capplet = NULL;
GtkWidget *delete_button = NULL;
GtkWidget *remove_button = NULL;
GtkWidget *add_button = NULL;
GtkWidget *info_frame = NULL;
GtkWidget *icon_entry, *extension_list, *mime_list;
GtkWidget *application_menu, *component_menu;
GtkWidget *none_button, *application_button, *component_button;

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
ok_callback ()
{
}

static void
populate_extension_list (const char *mime_type, GtkCList *list)
{
	GList *extensions, *element;
	gchar *extension[1];
	gint row;
		
	if (mime_type == NULL || list == NULL) {
		return;
	}
		
	/* Clear out old items */
	gtk_clist_clear (list);

	extensions = gnome_vfs_mime_get_extensions (mime_type);
	if (extensions == NULL) {
		return;
	}

	for (element = extensions; element != NULL; element = element->next) {
		extension[0] = (char *)element->data;
		if (strlen (element->data) > 0) {
			row = gtk_clist_append (list, extension);
			/* Set to deletable */
			gtk_clist_set_row_data (list, row, GINT_TO_POINTER (TRUE));
		}
	}

	gnome_vfs_mime_extension_list_free (extensions);	
	
	/* Select first item in extension list */
	gtk_clist_select_row (list, 0, 0);
}

void
nautilus_mime_type_capplet_add_extension (const char *extension)
{
	gchar *title[1];
	gchar *token, *search_string;
	gint rownumber;
	const char *mime_type;

	/* Check for empty string */
	if (strlen (extension) <= 0) {
		return;
	}

	/* Check for starting space in string */
	if (extension[0] == ' ') {
		return;
	}
	
	/* Copy only contiguous part of string.  No spaces allowed. */	
	search_string = g_strdup (extension);
	token = strtok (search_string, " ");

	if (token == NULL) {		
		title[0] = g_strdup (extension);
	} else if (strlen (token) <= 0) {
		return;
	}else {
		title[0] = g_strdup (token);		
	}
	g_free (search_string);
	
	rownumber = gtk_clist_append (GTK_CLIST (extension_list), title);
	gtk_clist_set_row_data (GTK_CLIST (extension_list), rownumber,
				GINT_TO_POINTER (TRUE));

	mime_type = nautilus_mime_type_capplet_get_selected_item_mime_type ();
	g_assert (mime_type != NULL);
	gnome_vfs_mime_add_extension (mime_type, extension);

	/* Select new item in list */
	gtk_clist_select_row (GTK_CLIST (extension_list), rownumber, 0);

}

static void
add_extension_clicked (GtkWidget *widget, gpointer data)
{
	nautilus_mime_type_capplet_show_new_extension_window ();
}

static void
remove_extension_clicked (GtkWidget *widget, gpointer data)
{
        gint row;
	gchar *text;
	gchar *store;
	const char *mime_type;
	
	text = (gchar *)g_malloc (sizeof (gchar) * 1024);
	gtk_clist_freeze (GTK_CLIST (extension_list));
	row = GPOINTER_TO_INT (GTK_CLIST (extension_list)->selection->data);
	gtk_clist_get_text (GTK_CLIST (extension_list), row, 0, &text);
	store = g_strdup (text);
	gtk_clist_remove (GTK_CLIST (extension_list), row);
	gtk_clist_thaw (GTK_CLIST (extension_list));


	mime_type = nautilus_mime_type_capplet_get_selected_item_mime_type ();
	if (mime_type != NULL) {
		gnome_vfs_mime_remove_extension (mime_type, store);
	}

	/* Select first item in list */
	gtk_clist_select_row (GTK_CLIST (extension_list), 0, 0);

	g_free (store);
}

static void
extension_list_selected (GtkWidget *clist, gint row, gint column, gpointer data)
{
        gboolean deletable;

	deletable = GPOINTER_TO_INT (gtk_clist_get_row_data (GTK_CLIST (clist), row));
	if (deletable)
	        gtk_widget_set_sensitive (remove_button, TRUE);
	else
	        gtk_widget_set_sensitive (remove_button, FALSE);
}

static void
extension_list_deselected (GtkWidget *clist, gint row, gint column, gpointer data)
{
        if (g_list_length (GTK_CLIST (clist)->selection) == 0)
	        gtk_widget_set_sensitive (remove_button, FALSE);
}

static void
mime_list_selected_row_callback (GtkWidget *widget, gint row, gint column, GdkEvent *event, gpointer data)
{
        const char *mime_type;
	
        if (column < 0)
                return;
        
        mime_type = (const char *) gtk_clist_get_row_data (GTK_CLIST (widget),row);

	/* Update info on selection */
        nautilus_mime_type_capplet_update_info (mime_type);
        
	/* FIXME: Get user mime info and determine if we can enable the delete button */
}

static void
none_button_toggled (GtkToggleButton *button, gpointer user_data)
{
	if (gtk_toggle_button_get_active (button)) {
		gnome_vfs_mime_set_default_action_type (nautilus_mime_type_capplet_get_selected_item_mime_type (), 
					GNOME_VFS_MIME_ACTION_TYPE_NONE);
	}
}

static void
application_button_toggled (GtkToggleButton *button, gpointer user_data)
{
	if (gtk_toggle_button_get_active (button)) {
		gnome_vfs_mime_set_default_action_type (nautilus_mime_type_capplet_get_selected_item_mime_type (), 
					GNOME_VFS_MIME_ACTION_TYPE_APPLICATION);
	}
}

static void
component_button_toggled (GtkToggleButton *button, gpointer user_data)
{
	if (gtk_toggle_button_get_active (button)) {
		gnome_vfs_mime_set_default_action_type (nautilus_mime_type_capplet_get_selected_item_mime_type (), 
					GNOME_VFS_MIME_ACTION_TYPE_COMPONENT);
	}
}


static void
init_mime_capplet (void)
{
	GtkWidget *main_vbox;
        GtkWidget *vbox, *hbox, *frame_vbox;
        GtkWidget *button;
        GtkWidget *label;
        GtkWidget *icon_entry;
        GtkWidget *mime_list_container;
        GtkWidget *extension_scroller;
        GtkWidget *action_frame;
        GtkWidget *table;
	
        gchar *title[2] = {"Extensions"};
        
	capplet = capplet_widget_new ();

	/* Main vertical box */                    
	main_vbox = gtk_vbox_new (FALSE, GNOME_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (main_vbox), GNOME_PAD_SMALL);
        gtk_container_add (GTK_CONTAINER (capplet), main_vbox);

        /* Main horizontal box and mime list*/                    
        hbox = gtk_hbox_new (FALSE, GNOME_PAD);
        gtk_box_pack_start (GTK_BOX (main_vbox), hbox, TRUE, TRUE, 0);
        mime_list_container = create_mime_list_and_scroller ();
        gtk_box_pack_start (GTK_BOX (hbox), mime_list_container, TRUE, TRUE, 0);         

	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
        gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

	/* Set up info frame */	
        info_frame = gtk_frame_new ("");
        gtk_box_pack_start (GTK_BOX (main_vbox), info_frame, FALSE, FALSE, 0);

	/* Create table */
	table = gtk_table_new (4, 4, FALSE);
	gtk_container_add (GTK_CONTAINER (info_frame), table);	
	gtk_table_set_row_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);
	gtk_table_set_col_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);
	
 	/* Create label, menu and edit button for default applications */
	label = gtk_label_new (_("Default Application:"));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_table_attach_defaults ( GTK_TABLE (table), label, 0, 1, 0, 1);
	
	application_menu = gtk_option_menu_new();
	gtk_table_attach_defaults ( GTK_TABLE (table), application_menu, 1, 2, 0, 1);

        button = left_aligned_button (_("Edit List"));
        gtk_table_attach_defaults ( GTK_TABLE (table), button, 2, 3, 0, 1);
	gtk_signal_connect (GTK_OBJECT (button), "clicked", edit_applications_clicked, mime_list);
	
	/* Create label, menu and edit button for default components */
	label = gtk_label_new (_("Default Viewer:"));
	gtk_table_attach_defaults ( GTK_TABLE (table), label, 0, 1, 1, 2);

	component_menu = gtk_option_menu_new();
	gtk_table_attach_defaults ( GTK_TABLE (table), component_menu, 1, 2, 1, 2);

        button = left_aligned_button (_("Edit List"));
	gtk_table_attach_defaults ( GTK_TABLE (table), button, 2, 3, 1, 2);
	gtk_signal_connect (GTK_OBJECT (button), "clicked", edit_components_clicked, mime_list);

	/* Add icon box */
	icon_entry = gnome_icon_entry_new ("mime_icon_entry", _("Select an icon..."));
	/*gtk_signal_connect (GTK_OBJECT (gnome_icon_entry_gtk_entry (GNOME_ICON_ENTRY (icon_entry))),
			    "changed", icon_changed, NULL);*/
	gtk_table_attach_defaults ( GTK_TABLE (table), icon_entry, 0, 1, 2, 4);

	/* Add extension list and buttons */
	extension_list = gtk_clist_new_with_titles (1, title);
	gtk_clist_column_titles_passive (GTK_CLIST (extension_list));
	gtk_clist_set_auto_sort (GTK_CLIST (extension_list), TRUE);
	gtk_clist_set_selection_mode (GTK_CLIST (extension_list), GTK_SELECTION_BROWSE);
        gtk_clist_set_auto_sort (GTK_CLIST (extension_list), TRUE);

	extension_scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (extension_scroller),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (extension_scroller), extension_list);

	gtk_widget_set_usize (extension_scroller, 150, 60);
	gtk_table_attach_defaults ( GTK_TABLE (table), extension_scroller, 1, 2, 2, 4);

        add_button = gtk_button_new_with_label (_("Add"));
	gtk_widget_set_sensitive (add_button, TRUE);
	gtk_table_attach_defaults ( GTK_TABLE (table), add_button, 2, 3, 2, 3);

        remove_button = gtk_button_new_with_label (_("Remove"));
	gtk_widget_set_sensitive (remove_button, FALSE);
	gtk_table_attach_defaults ( GTK_TABLE (table), remove_button, 2, 3, 3, 4);

	gtk_signal_connect (GTK_OBJECT (remove_button), "clicked",
			    GTK_SIGNAL_FUNC (remove_extension_clicked), NULL);
	gtk_signal_connect (GTK_OBJECT (add_button), "clicked",
			    GTK_SIGNAL_FUNC (add_extension_clicked), NULL);
	gtk_signal_connect (GTK_OBJECT (extension_list), "select-row",
			    GTK_SIGNAL_FUNC (extension_list_selected), NULL);
	gtk_signal_connect (GTK_OBJECT (extension_list), "unselect-row",
			    GTK_SIGNAL_FUNC (extension_list_deselected), NULL);

	/* Default Action frame */
	action_frame = gtk_frame_new (_("Default Action"));
	gtk_table_attach_defaults ( GTK_TABLE (table), action_frame, 3, 4, 0, 4);

	frame_vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (action_frame), frame_vbox);

	none_button = gtk_radio_button_new_with_label (NULL, _("No Default"));
	gtk_box_pack_start (GTK_BOX (frame_vbox), none_button, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (none_button), "toggled",
			    GTK_SIGNAL_FUNC (none_button_toggled), NULL);

	application_button = gtk_radio_button_new_with_label_from_widget ( 
		           GTK_RADIO_BUTTON (none_button), _("Use Application"));
	gtk_box_pack_start (GTK_BOX (frame_vbox), application_button, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (application_button), "toggled",
			    GTK_SIGNAL_FUNC (application_button_toggled), NULL);

	component_button = gtk_radio_button_new_with_label_from_widget ( 
		           GTK_RADIO_BUTTON (application_button), _("Use Viewer"));
	gtk_box_pack_start (GTK_BOX (frame_vbox), component_button, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (component_button), "toggled",
			    GTK_SIGNAL_FUNC (component_button_toggled), NULL);

	/* Mime list Add and Delete buttons */
        button = left_aligned_button (_("Add..."));
        gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (button), "clicked", add_mime_clicked, NULL);
        
	delete_button = left_aligned_button (_("Delete"));
        gtk_signal_connect (GTK_OBJECT (delete_button), "clicked", delete_mime_clicked, NULL);
        gtk_box_pack_start (GTK_BOX (vbox), delete_button, FALSE, FALSE, 0);

	/* Set up enabled/disabled states of capplet buttons */

	/* Yes, show all widgets */
        gtk_widget_show_all (capplet);

        /* Setup capplet signals */
        gtk_signal_connect(GTK_OBJECT(capplet), "ok",
                           GTK_SIGNAL_FUNC(ok_callback), NULL);

	gtk_signal_connect (GTK_OBJECT (mime_list),"select_row",
       	                   GTK_SIGNAL_FUNC (mime_list_selected_row_callback), NULL);

	/* Select first item in list and update menus */
	gtk_clist_select_row (GTK_CLIST (mime_list), 0, 0);

	capplet_widget_state_changed (CAPPLET_WIDGET (capplet), FALSE);
}


/*
 *  nautilus_mime_type_capplet_update_info
 *
 *  Update controls with info based on mime type	
 */
 
void
nautilus_mime_type_capplet_update_info (const char *mime_type) {

	GnomeVFSMimeAction *action;

	/* Update frame label with mime type */
	gtk_frame_set_label (GTK_FRAME (info_frame), mime_type);
	
	/* Update menus */
	populate_application_menu (application_menu, mime_type);
	populate_component_menu (component_menu, mime_type);

	/* Update extesnions list */
	populate_extension_list (mime_type, GTK_CLIST (extension_list));

	/* Set icon for mime type */
	gnome_icon_entry_set_icon (GNOME_ICON_ENTRY (icon_entry),
				   gnome_vfs_mime_get_value (mime_type, "icon-filename"));

	/* Indicate default action */	
	action = gnome_vfs_mime_get_default_action (mime_type);
	if (action != NULL) {
		switch (action->action_type) {
			case GNOME_VFS_MIME_ACTION_TYPE_NONE:
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (none_button), TRUE);
				break;

			case GNOME_VFS_MIME_ACTION_TYPE_APPLICATION:
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (application_button), TRUE);
				break;

			case GNOME_VFS_MIME_ACTION_TYPE_COMPONENT:
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (component_button), TRUE);
				break;
				
			default:
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (none_button), TRUE);
				break;
		}
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (none_button), TRUE);
	}
}

static void 
application_menu_activated (GtkWidget *menu_item, gpointer data)
{
	const char *id;
	const char *mime_type;

	/* Get our stashed data */
	id = gtk_object_get_data (GTK_OBJECT (menu_item), "id");
	mime_type = gtk_object_get_data (GTK_OBJECT (menu_item), "mime_type");

	if (id == NULL || mime_type == NULL) {
		return;
	}	
	gnome_vfs_mime_set_default_application (mime_type, id);
}

static void
populate_application_menu (GtkWidget *application_menu, const char *mime_type)
{
	GtkWidget *new_menu, *menu_item;
	GList *app_list, *copy_list;
	GnomeVFSMimeApplication *default_app, *application;
	gboolean has_none, found_match;
	char *mime_copy;
	const char *id;
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

			/* Store copy of application name and mime type in item; free when item destroyed. */
			gtk_object_set_data_full (GTK_OBJECT (menu_item),
						  "id",
						  g_strdup (application->id),
						  g_free);

			gtk_object_set_data_full (GTK_OBJECT (menu_item),
						  "mime_type",
						  g_strdup (mime_type),
						  g_free);
			
			gtk_menu_append (GTK_MENU (new_menu), menu_item);
			gtk_widget_show (menu_item);

			gtk_signal_connect (GTK_OBJECT (menu_item), "activate", 
					    application_menu_activated, NULL);
		}
	
		gnome_vfs_mime_application_list_free (app_list);
	}

	/* Find all applications or add a "None" item */
	if (has_none && default_app == NULL) {		
		menu_item = gtk_menu_item_new_with_label (_("None"));
		gtk_menu_append (GTK_MENU (new_menu), menu_item);
		gtk_widget_show (menu_item);
	} else {
		/* Check and see if default is in the short list */
		if (default_app != NULL) {
			/* Iterate */
			children = gtk_container_children (GTK_CONTAINER (new_menu));
			for (index = 0; children != NULL; children = children->next, ++index) {				
				id = (const char *)(gtk_object_get_data (GTK_OBJECT (children->data), "id"));
				if (id != NULL) {											
					if (strcmp (default_app->id, id) == 0) {
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
				/* No match found.  We need to insert a menu item
				 * and add the application to the default list */
				menu_item = gtk_menu_item_new_with_label (default_app->name);

				/* Store copy of application name and mime type in item; free when item destroyed. */
				gtk_object_set_data_full (GTK_OBJECT (menu_item),
							  "id",
							  g_strdup (default_app->id),
							  g_free);

				gtk_object_set_data_full (GTK_OBJECT (menu_item),
							  "mime_type",
							  g_strdup (mime_type),
							  g_free);
				
				gtk_menu_append (GTK_MENU (new_menu), menu_item);
				gtk_widget_show (menu_item);

				gtk_signal_connect (GTK_OBJECT (menu_item), "activate", 
						    application_menu_activated, NULL);
				
				
				/* Iterate */
				children = gtk_container_children (GTK_CONTAINER (new_menu));
				for (index = 0; children != NULL; children = children->next, ++index) {				
					id = (const char *)(gtk_object_get_data (GTK_OBJECT (children->data), "id"));
					if (id != NULL) {											
						if (strcmp (default_app->id, id) == 0) {
							found_match = TRUE;
							break;
						}
					}
				}
				g_list_free (children);

				/* Set it as active */
				gtk_menu_set_active (GTK_MENU (new_menu), index);
			}
		}
	}
	
	gtk_option_menu_set_menu (GTK_OPTION_MENU (application_menu), new_menu);
}


static void 
component_menu_activated (GtkWidget *menu_item, gpointer data)
{
	const char *iid;
	const char *mime_type;

	/* Get our stashed data */
	iid = gtk_object_get_data (GTK_OBJECT (menu_item), "iid");
	mime_type = gtk_object_get_data (GTK_OBJECT (menu_item), "mime_type");

	if (iid == NULL || mime_type == NULL) {
		return;
	}

	gnome_vfs_mime_set_default_component (mime_type, iid);
}

static void
populate_component_menu (GtkWidget *component_menu, const char *mime_type)
{
	GtkWidget *new_menu;
	GtkWidget *menu_item;
	GList *component_list, *copy_list;
	GList *list_element;
	gboolean has_none, found_match;
	char *component_name;
	OAF_ServerInfo *default_component;
	OAF_ServerInfo *info;
	const char *iid;
	GList *children;
	int index;
	
	has_none = TRUE;
	found_match = FALSE;

	new_menu = gtk_menu_new ();
	
	/* Get the default component */
	default_component = gnome_vfs_mime_get_default_component (mime_type);

	/* Fill list with default components */
	component_list = gnome_vfs_mime_get_short_list_components (mime_type);
	if (component_list != NULL) {
		for (list_element = component_list; list_element != NULL; list_element = list_element->next) {
			has_none = FALSE;

			component_name = name_from_oaf_server_info (list_element->data);
			menu_item = gtk_menu_item_new_with_label (component_name);

			/* Store copy of component name and mime type in item; free when item destroyed. */
			info = list_element->data;
			gtk_object_set_data_full (GTK_OBJECT (menu_item),
						  "iid",
						  g_strdup (info->iid),
						  g_free);

			gtk_object_set_data_full (GTK_OBJECT (menu_item),
						  "mime_type",
						  g_strdup (mime_type),
						  g_free);
			
			gtk_menu_append (GTK_MENU (new_menu), menu_item);
			gtk_widget_show (menu_item);

			gtk_signal_connect (GTK_OBJECT (menu_item), "activate", 
					    component_menu_activated, NULL);
		}

		gnome_vfs_mime_component_list_free (component_list);
	}
	
	/* Add a "None" item */
	if (has_none && default_component == NULL) {		
		menu_item = gtk_menu_item_new_with_label (_("None"));
		gtk_menu_append (GTK_MENU (new_menu), menu_item);
		gtk_widget_show (menu_item);
	} else {
		/* Check and see if default is in the short list */
		if (default_component != NULL) {
			/* Iterate */
			children = gtk_container_children (GTK_CONTAINER (new_menu));
			for (index = 0; children != NULL; children = children->next, ++index) {				
				iid = (const char *)(gtk_object_get_data (GTK_OBJECT (children->data), "iid"));
				if (iid != NULL) {											
					if (strcmp (default_component->iid, iid) == 0) {
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
				/* No match found.  We need to insert a menu item
				 * and add the application to the default list */

				component_name = name_from_oaf_server_info (copy_list->data);
				menu_item = gtk_menu_item_new_with_label (component_name);


				/* Store copy of application name and mime type in item; free when item destroyed. */
				gtk_object_set_data_full (GTK_OBJECT (menu_item),
							  "iid",
							  g_strdup (default_component->iid),
							  g_free);

				gtk_object_set_data_full (GTK_OBJECT (menu_item),
							  "mime_type",
							  g_strdup (mime_type),
							  g_free);
				
				gtk_menu_append (GTK_MENU (new_menu), menu_item);
				gtk_widget_show (menu_item);

				gtk_signal_connect (GTK_OBJECT (menu_item), "activate", 
						    component_menu_activated, NULL);
				
				
				/* Iterate */
				children = gtk_container_children (GTK_CONTAINER (new_menu));
				for (index = 0; children != NULL; children = children->next, ++index) {				
					iid = (const char *)(gtk_object_get_data (GTK_OBJECT (children->data), "iid"));
					if (iid != NULL) {											
						if (strcmp (default_component->iid, iid) == 0) {
							found_match = TRUE;
							break;
						}
					}
				}
				g_list_free (children);

				/* Set it as active */
				gtk_menu_set_active (GTK_MENU (new_menu), index);
			}
		}
	}
	
	gtk_option_menu_set_menu (GTK_OPTION_MENU (component_menu), new_menu);
}


/*
 *  delete_mime_clicked	
 *
 *  Delete mime type if it is a user defined type.
 */
 
static void
delete_mime_clicked (GtkWidget *widget, gpointer data)
{
        const char *mime_type;
        gint row = 0;

        if (GTK_CLIST (mime_list)->selection)
                row = GPOINTER_TO_INT ((GTK_CLIST (mime_list)->selection)->data);
        else
                return;
        mime_type = (const char *) gtk_clist_get_row_data (GTK_CLIST (mime_list), row);

        gtk_clist_remove (GTK_CLIST (mime_list), row);
	/* FIXME: Get user mime info */
        //g_hash_table_remove (user_mime_types, mi->mime_type);
	//remove_mime_info (mi->mime_type);
}

static void
add_mime_clicked (GtkWidget *widget, gpointer data)
{
	nautilus_mime_type_capplet_show_new_mime_window ();
}

static void
edit_applications_clicked (GtkWidget *widget, gpointer data)
{	
	GtkWidget *list;
	const char *mime_type;
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
	mime_type = (const char *) row->data;
	show_edit_applications_dialog (mime_type);
}

static void
edit_components_clicked (GtkWidget *widget, gpointer data)
{
	GtkWidget *list;
	const char *mime_type;
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
	mime_type = (const char *) row->data;
	show_edit_components_dialog (mime_type);
}

/*
 * nautilus_mime_type_capplet_update_application_info
 * 
 * Update state of the applications menu.  This function is called
 * when the Edit Applications dialog is closed with an OK.
 */
 
void
nautilus_mime_type_capplet_update_application_info (const char *mime_type)
{
	populate_application_menu (application_menu, mime_type);
}

/*
 * nautilus_mime_type_capplet_update_component_info
 * 
 * Update state of the components menu.  This function is called
 * when the Edit Componests dialog is closed with an OK.
 */
 
void
nautilus_mime_type_capplet_update_component_info (const char *mime_type)
{
	populate_component_menu (component_menu, mime_type);
}


static void
insert_mime_vals_into_clist (GList *type_list, GtkCList *clist)
{
	static gchar *text[2];        
	const char *description;
	char *mime_string;
        gint row;
	GList *element;
	
	for (element = type_list; element != NULL; element= element->next) {
		mime_string = (char *)element->data;
		
		/* Make sure we do not include comments */
		if (mime_string[0] != '#') {
			/* Add mime type to first column */
			text[0] = mime_string;

			/* Add description to second column */
			description = gnome_vfs_mime_description (mime_string);	
			if (description != NULL && strlen (description) > 0) {
				text[1] = g_strdup (description);
			} else {
				text[1] = g_strdup ("");
			}
			
		        row = gtk_clist_insert (GTK_CLIST (clist), 1, text);
		        gtk_clist_set_row_data (GTK_CLIST (clist), row, g_strdup(mime_string));

			g_free (text[1]);
		}
	}
}

static GtkWidget *
create_mime_list_and_scroller (void)
{
        GtkWidget *window;
        gchar *titles[3];
	GList *type_list;
	
        titles[0] = _("Mime Type");
        titles[1] = _("Description");
        titles[2] = _("Application");
        
        window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
	mime_list = gtk_clist_new_with_titles (3, titles);
        gtk_clist_set_selection_mode (GTK_CLIST (mime_list), GTK_SELECTION_BROWSE);
        gtk_clist_set_auto_sort (GTK_CLIST (mime_list), TRUE);

	type_list = gnome_vfs_get_registered_mime_types ();
	insert_mime_vals_into_clist (type_list, GTK_CLIST (mime_list));
	gnome_vfs_mime_registered_mime_type_list_free (type_list);
	
        gtk_clist_columns_autosize (GTK_CLIST (mime_list));
        gtk_clist_select_row (GTK_CLIST (mime_list), 0, 0);
        gtk_container_add (GTK_CONTAINER (window), mime_list);

        return window;
}

const char * 
nautilus_mime_type_capplet_get_selected_item_mime_type (void)
{
	const char *mime_type;
	int row;
	GtkCList *clist;

	clist = GTK_CLIST (mime_list);
	
	if (clist->selection == NULL) {
		return NULL;
	}

	/* This is a single selection list, so we just use the first item in 
	 * the list to retireve the data */
	row = GPOINTER_TO_INT (clist->selection->data);

	mime_type = (const char *) gtk_clist_get_row_data (clist, row);

	return mime_type;
}
