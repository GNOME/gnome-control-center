/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-mime-type-capplet-dialog.c
 *
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

#include <capplet-widget.h>
#include <gnome.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>

#include "nautilus-mime-type-capplet.h"
#include "nautilus-mime-type-capplet-dialogs.h"

/* gtk_window_set_default_width (and some other functions) use a
 * magic undocumented number of -2 to mean "ignore this parameter".
 */
#define NO_DEFAULT_MAGIC_NUMBER		-2

/* Scrolling list has no idea how tall to make itself. Its
 * "natural height" is just enough to draw the scroll bar controls.
 * Hardwire an initial window size here, but let user resize
 * bigger or smaller.
 */
#define PROGRAM_CHOOSER_DEFAULT_HEIGHT	 280


typedef struct {
	GtkWidget *window;
	GtkWidget *preferred_list;
	GtkWidget *default_list;
} edit_dialog_details;


/* Global variables */
static edit_dialog_details *edit_application_details = NULL;
static edit_dialog_details *edit_component_details = NULL;
extern GtkWidget *capplet;

static void
edit_applications_dialog_destroy (GtkWidget *widget, gpointer data)
{
	g_free (edit_application_details);
	edit_application_details = NULL;
}

static void
edit_component_dialog_destroy (GtkWidget *widget, gpointer data)
{
	g_free (edit_component_details);
	edit_component_details = NULL;
}

static gboolean 
application_is_in_list (const char *search_id, GList *application_list)
{
	GList *list_element;
	GnomeVFSMimeApplication *application;

	if (application_list == NULL || search_id == NULL) {
		return FALSE;
	}

	/* Traverse the list looking for a match */
	for (list_element = application_list; list_element != NULL; list_element = list_element->next) {
		application = list_element->data;

		if (strcmp (search_id, application->id) == 0) {
			return TRUE;			
		}
	}
	
	return FALSE;
}


/*
 * application_button_toggled_callback
 *
 * Check state of button.  Based on state, determine whether to add or remove
 * application from short list.
 */
static void 
application_button_toggled_callback (GtkToggleButton *button, gpointer user_data) 
{
	const char *id;
	const char *mime_type;

	id = gtk_object_get_data (GTK_OBJECT (button), "application_id");
	mime_type = gtk_object_get_data (GTK_OBJECT (button), "mime_type");

	if (id == NULL || mime_type == NULL) {
		return;
	}

	if (gtk_toggle_button_get_active (button)) {
		/* Add to preferred list */
		gnome_vfs_mime_add_application_to_short_list (mime_type, id);

	} else {
		/* Remove from preferred list */
		gnome_vfs_mime_remove_application_from_short_list (mime_type, id);
	}
}

static void
populate_default_applications_box (GtkWidget *box, const char *mime_type)
{
	GList *short_list, *app_list, *list_element;
	GnomeVFSMimeApplication *application;
	GtkWidget *button;

	/* Get the application short list */
	short_list = gnome_vfs_mime_get_short_list_applications (mime_type);

	/* Get the list of all applications */
	app_list = gnome_vfs_mime_get_all_applications (mime_type);
	if (app_list != NULL) {
		for (list_element = app_list; list_element != NULL; list_element = list_element->next) {
			application = list_element->data;

			/* Create check button */
			button = gtk_check_button_new_with_label (application->name);
			gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);

			/* Save ID and mime type*/
			gtk_object_set_data_full (GTK_OBJECT (button), "application_id", g_strdup (application->id), g_free);
			gtk_object_set_data_full (GTK_OBJECT (button), "mime_type", g_strdup (mime_type), g_free);
			
			/* Check and see if component is in preferred list */
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
						      application_is_in_list (application->id, short_list));

			/* Connect to toggled signal */
			gtk_signal_connect (GTK_OBJECT (button), "toggled", 
					    GTK_SIGNAL_FUNC (application_button_toggled_callback), NULL);
		}

		gnome_vfs_mime_application_list_free (app_list);
		
	}
	
	if (short_list != NULL) {
		gnome_vfs_mime_application_list_free (short_list);
	}
}


static gboolean 
component_is_in_list (const char *search_name, GList *component_list)
{
	GList *list_element;
	OAF_ServerInfo *info;
	gchar *component_name;

	if (component_list == NULL || search_name == NULL) {
		return FALSE;
	}

	/* Traverse the list looking for a match */
	for (list_element = component_list; list_element != NULL; list_element = list_element->next) {
		info = list_element->data;

		component_name = name_from_oaf_server_info (info);
		
		if (strcmp (search_name, component_name) == 0) {			
			g_free (component_name);
			return TRUE;			
		}
		g_free (component_name);
	}
	
	return FALSE;
}

/*
 * component_button_toggled_callback
 *
 * Check state of button.  Based on state, determine whether to add or remove
 * component from short list.
 */
static void 
component_button_toggled_callback (GtkToggleButton *button, gpointer user_data) 
{
	const char *iid;
	const char *mime_type;

	iid = gtk_object_get_data (GTK_OBJECT (button), "component_iid");
	mime_type = gtk_object_get_data (GTK_OBJECT (button), "mime_type");

	if (iid == NULL || mime_type == NULL) {
		return;
	}

	if (gtk_toggle_button_get_active (button)) {
		/* Add to preferred list */
		gnome_vfs_mime_add_component_to_short_list (mime_type, iid);

	} else {
		/* Remove from preferred list */
		gnome_vfs_mime_remove_component_from_short_list (mime_type, iid);
	}
}


static void
populate_default_components_box (GtkWidget *box, const char *mime_type)
{
	GList *short_component_list;
	GList *all_component_list, *list_element;
	OAF_ServerInfo *info;
	gchar *component_name;
	GtkWidget *button;

	/* Get short list of components */
	short_component_list = gnome_vfs_mime_get_short_list_components (mime_type);
		
	/* Get list of all components */
	all_component_list = gnome_vfs_mime_get_all_components (mime_type);
	if (all_component_list != NULL) {
		for (list_element = all_component_list; list_element != NULL; list_element = list_element->next) {
			info = list_element->data;

			component_name = name_from_oaf_server_info (info);
			button = gtk_check_button_new_with_label (component_name);
			gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
			
			/* Save IID and mime type*/
			gtk_object_set_data_full (GTK_OBJECT (button), "component_iid", g_strdup (info->iid), g_free);
			gtk_object_set_data_full (GTK_OBJECT (button), "mime_type", g_strdup (mime_type), g_free);
			
			/* Check and see if component is in preferred list */
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button),
						      component_is_in_list (component_name, short_component_list));

			/* Connect to toggled signal */
			gtk_signal_connect (GTK_OBJECT (button), "toggled", 
					    GTK_SIGNAL_FUNC (component_button_toggled_callback), NULL);

			g_free (component_name);
		}
		gnome_vfs_mime_component_list_free (all_component_list);

		if (short_component_list != NULL) {
			gnome_vfs_mime_component_list_free (short_component_list);
		}
	}	
}


/*
 *  initialize_edit_applications_dialog
 *  
 *  Set up dialog for default application list editing
 */
 
static void
initialize_edit_applications_dialog (const char *mime_type)
{
	GtkWidget *main_vbox, *vbox;
	GtkWidget *scroller, *label;
	char *label_text;
	
	edit_application_details = g_new0 (edit_dialog_details, 1);

	edit_application_details->window = gnome_dialog_new (_("Edit Applications List"),
					     GNOME_STOCK_BUTTON_OK,
					     GNOME_STOCK_BUTTON_CANCEL,
					     NULL);

	gtk_container_set_border_width (GTK_CONTAINER (edit_application_details->window), GNOME_PAD);
  	gtk_window_set_policy (GTK_WINDOW (edit_application_details->window), FALSE, TRUE, FALSE);
	gtk_window_set_default_size (GTK_WINDOW (edit_application_details->window), 
				     NO_DEFAULT_MAGIC_NUMBER,
				     PROGRAM_CHOOSER_DEFAULT_HEIGHT);

	gtk_signal_connect (GTK_OBJECT (edit_application_details->window),
			    "destroy",
			    edit_applications_dialog_destroy,
			    NULL);

	/* Main vertical box */
	main_vbox = GNOME_DIALOG (edit_application_details->window)->vbox;			    

	/* Add label */
	label_text = g_strdup_printf (_("Select applications to appear in menu for mime type \"%s\""), mime_type);
	label = gtk_label_new (label_text);
	g_free (label_text);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start (GTK_BOX (main_vbox), label, FALSE, FALSE, 0);
	
	/* Add scrolling list of check buttons */
	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_box_pack_start_defaults (GTK_BOX (main_vbox), scroller);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);

	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scroller), vbox);

	populate_default_applications_box (vbox, mime_type);

	gtk_widget_show_all (main_vbox);
}



/*
 *  initialize_edit_components_dialog
 *  
 *  Set up dialog for default component list editing
 */
 
static void
initialize_edit_components_dialog (const char *mime_type)
{
	GtkWidget *main_vbox, *vbox;
	GtkWidget *scroller, *label;
	char *label_text;
	
	edit_component_details = g_new0 (edit_dialog_details, 1);

	edit_component_details->window = gnome_dialog_new (_("Edit Components List"),
					     GNOME_STOCK_BUTTON_OK,
					     GNOME_STOCK_BUTTON_CANCEL,
					     NULL);

	gtk_container_set_border_width (GTK_CONTAINER (edit_component_details->window), GNOME_PAD);
  	gtk_window_set_policy (GTK_WINDOW (edit_component_details->window), FALSE, TRUE, FALSE);
	gtk_window_set_default_size (GTK_WINDOW (edit_component_details->window), 
				     NO_DEFAULT_MAGIC_NUMBER,
				     PROGRAM_CHOOSER_DEFAULT_HEIGHT);

	gtk_signal_connect (GTK_OBJECT (edit_component_details->window),
			    "destroy",
			    edit_component_dialog_destroy,
			    NULL);

	/* Main vertical box */
	main_vbox = GNOME_DIALOG (edit_component_details->window)->vbox;			    

	/* Add label */
	label_text = g_strdup_printf (_("Select views to appear in menu for mime type \"%s\""), mime_type);
	label = gtk_label_new (label_text);
	g_free (label_text);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start (GTK_BOX (main_vbox), label, FALSE, FALSE, 0);
	
	/* Add scrolling list of check buttons */
	scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_box_pack_start_defaults (GTK_BOX (main_vbox), scroller);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller),
					GTK_POLICY_NEVER,
					GTK_POLICY_AUTOMATIC);

	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scroller), vbox);

	populate_default_components_box (vbox, mime_type);

	gtk_widget_show_all (main_vbox);
}


/*
 *  show_edit_applications_dialog
 *  
 *  Setup and display edit application list dialog
 */

void
show_edit_applications_dialog (const char *mime_type)
{	
	if (edit_application_details == NULL) {
		initialize_edit_applications_dialog (mime_type);
	}
	
	switch(gnome_dialog_run (GNOME_DIALOG (edit_application_details->window))) {
		case 0:
			nautilus_mime_type_capplet_update_application_info (mime_type);
			/* Delete the dialog so the lists are repopulated on next lauch */
			gtk_widget_destroy (edit_application_details->window);
			break;
			
		case 1:
			/* Delete the dialog so the lists are repopulated on next lauch */
			gtk_widget_destroy (edit_application_details->window);
			break;
	}
}


/*
 *  show_edit_components_dialog
 *  
 *  Setup and display edit component list dialog
 */

void
show_edit_components_dialog (const char *mime_type)
{	
	if (edit_component_details == NULL) {
		initialize_edit_components_dialog (mime_type);
	}

	switch(gnome_dialog_run (GNOME_DIALOG (edit_component_details->window))) {
		case 0:
			nautilus_mime_type_capplet_update_component_info (mime_type);
			/* Delete the dialog so the lists are repopulated on next lauch */
			gtk_widget_destroy (edit_component_details->window);			
			break;
			
		case 1:
			/* Delete the dialog so the lists are repopulated on next lauch */
			gtk_widget_destroy (edit_component_details->window);
			break;
	}
}


static GSList *
get_lang_list (void)
{
        GSList *retval;
        char *lang;
        char * equal_char;

        retval = NULL;

        lang = g_getenv ("LANGUAGE");

        if (!lang) {
                lang = g_getenv ("LANG");
        }


        if (lang) {
                equal_char = strchr (lang, '=');
                if (equal_char != NULL) {
                        lang = equal_char + 1;
                }

                retval = g_slist_prepend (retval, lang);
        }
        
        return retval;
}

static gboolean
str_has_prefix (const char *haystack, const char *needle)
{
	const char *h, *n;

	/* Eat one character at a time. */
	h = haystack == NULL ? "" : haystack;
	n = needle == NULL ? "" : needle;
	do {
		if (*n == '\0') {
			return TRUE;
		}
		if (*h == '\0') {
			return FALSE;
		}
	} while (*h++ == *n++);
	return FALSE;
}

char *
name_from_oaf_server_info (OAF_ServerInfo *server)
{
        const char *view_as_name;       
	char *display_name;
        GSList *langs;

        display_name = NULL;

        langs = get_lang_list ();
        view_as_name = oaf_server_info_attr_lookup (server, "nautilus:view_as_name", langs);
		
        if (view_as_name == NULL) {
                view_as_name = oaf_server_info_attr_lookup (server, "name", langs);
        }

        if (view_as_name == NULL) {
                view_as_name = server->iid;
        }
       
        g_slist_free (langs);

	/* if the name is an OAFIID, clean it up for display */
	if (str_has_prefix (view_as_name, "OAFIID:")) {
		char *display_name, *colon_ptr;		
		display_name = g_strdup (view_as_name + 7);
		colon_ptr = strchr (display_name, ':');
		if (colon_ptr) {
			*colon_ptr = '\0';
		}
		return display_name;					
	}

			
        return g_strdup(view_as_name);
}

void
nautilus_mime_type_capplet_show_new_mime_window (void)
{
	GtkWidget *dialog;
        GtkWidget *mime_entry;
	GtkWidget *label;
	GtkWidget *ext_entry;
	GtkWidget *hbox;
	GtkWidget *vbox;
	
        dialog = gnome_dialog_new (_("Add Mime Type"), GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
	label = gtk_label_new (_("Add a new Mime Type\nFor example:  image/tiff; text/x-scheme"));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), hbox, FALSE, FALSE, 0);
	label = gtk_label_new (_("Mime Type:"));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	mime_entry = gtk_entry_new ();
        gtk_box_pack_start (GTK_BOX (hbox), mime_entry, TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), hbox, FALSE, FALSE, 0);
	
        
	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), vbox, FALSE, FALSE, 0);
	//gtk_container_add (GTK_CONTAINER (frame), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), GNOME_PAD_SMALL);
	label = gtk_label_new (_("Type in a description for this mime-type."));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);	
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_("Description:")), FALSE, FALSE, 0);
	ext_entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), ext_entry, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	
	
        gtk_widget_show_all (GNOME_DIALOG (dialog)->vbox);
        switch (gnome_dialog_run (GNOME_DIALOG (dialog))) {
        case 0:
		capplet_widget_state_changed (CAPPLET_WIDGET (capplet),
					      TRUE);
                add_new_mime_type (gtk_entry_get_text (GTK_ENTRY (mime_entry)),
				   gtk_entry_get_text (GTK_ENTRY (ext_entry)));
        case 1:
                gtk_widget_destroy (dialog);
        default:;
        }
}

void
nautilus_mime_type_capplet_show_new_extension_window (void)
{
        GtkWidget *mime_entry;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *dialog;
	
        dialog = gnome_dialog_new (_("Add New Extension"), GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);
	label = gtk_label_new (_("Type in the extensions for this mime-type.\nFor example:  .html, .htm"));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), hbox, FALSE, FALSE, 0);
	label = gtk_label_new (_("Extension:"));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	mime_entry = gtk_entry_new ();
        gtk_box_pack_start (GTK_BOX (hbox), mime_entry, TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), hbox, FALSE, FALSE, 0);	
	
        gtk_widget_show_all (GNOME_DIALOG (dialog)->vbox);
        
        switch (gnome_dialog_run (GNOME_DIALOG (dialog))) {
	        case 0:
			capplet_widget_state_changed (CAPPLET_WIDGET (capplet),
						      TRUE);

			nautilus_mime_type_capplet_add_extension (gtk_entry_get_text 
							         (GTK_ENTRY (mime_entry)));

	        case 1:
	                gtk_widget_destroy (dialog);
	                break;
	                
	        default:
	        	break;
	}        
}