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
#include <ctype.h>
#include <gnome.h>
#include <gtk/gtk.h>
#include <gtk/gtklist.h>
#include <gtk/gtkbin.h>
#include <gtk/gtklistitem.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-application-registry.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "libuuid/uuid.h"
#include "file-types-capplet.h"
#include "file-types-capplet-dialogs.h"

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

/* Local prototypes */
static void 	show_new_application_window  (GtkWidget *button, GtkWidget *list);
static void 	show_edit_application_window (GtkWidget *button, GtkWidget *list);
static void 	delete_selected_application  (GtkWidget *button, GtkWidget *list);
static void	add_item_to_application_list (GtkWidget *list, const char *id, const char *name, const char *mime_type,
					      gboolean user_owned, int position);
static void	find_message_label_callback  (GtkWidget *widget, gpointer callback_data);
static void	find_message_label 	     (GtkWidget *widget, const char *message);


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
insert_item (GtkList *list_widget, GtkListItem *item, int position)
{
	GList *singleton_list;

	g_assert (GTK_IS_LIST (list_widget));
	g_assert (GTK_IS_LIST_ITEM (item));

	/* Due to GTK inheritance stupidity, the "Add" signal, which we
	 * rely on for widget sensitivity updates, is not sent if you
	 * use the GtkList API to add items. So when we add new items,
	 * which always go at the end, we must use the GtkContainer API.
	 */
	if (position < 0) {
		gtk_container_add (GTK_CONTAINER (list_widget), GTK_WIDGET (item));
	} else {
		singleton_list = g_list_prepend (NULL, item);
		gtk_list_insert_items (list_widget, singleton_list, position);
		/* This looks like a leak of a singleton_list, but believe it or not
		 * gtk_list takes ownership of the list of items.
		 */
	}
}

static GtkListItem *
create_application_list_item (const char *id, const char *name, const char *mime_type,
			      gboolean user_owned, GList *short_list)
{
	GtkWidget *list_item;
	GtkWidget *hbox, *check_button, *label;

	list_item = gtk_list_item_new ();
	
	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (list_item), hbox);
	
	check_button = gtk_check_button_new ();
	gtk_box_pack_start (GTK_BOX (hbox), check_button, FALSE, FALSE, 0);	

	label = gtk_label_new (name);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);	

	gtk_widget_show_all (list_item);
	
	/* Save ID and mime type*/	
	gtk_object_set_data_full (GTK_OBJECT (check_button), "application_id", g_strdup (id), g_free);
	gtk_object_set_data_full (GTK_OBJECT (check_button), "mime_type", g_strdup (mime_type), g_free);
	gtk_object_set_data_full (GTK_OBJECT (list_item), "application_id", g_strdup (id), g_free);
	gtk_object_set_data_full (GTK_OBJECT (list_item), "mime_type", g_strdup (mime_type), g_free);
	gtk_object_set_data (GTK_OBJECT (list_item), "user_owned", GINT_TO_POINTER(user_owned));

	/* Check and see if component is in preferred list */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button),
				      application_is_in_list (id, short_list));

	/* Connect to toggled signal */
	gtk_signal_connect (GTK_OBJECT (check_button), "toggled", 
			    GTK_SIGNAL_FUNC (application_button_toggled_callback), NULL);

	return GTK_LIST_ITEM (list_item);	
}

static void
populate_default_applications_list (GtkWidget *list, const char *mime_type)
{
	GList *short_list, *app_list, *list_element;
	GnomeVFSMimeApplication *application;
	GtkListItem *list_item;

	/* Get the application short list */
	short_list = gnome_vfs_mime_get_short_list_applications (mime_type);

	/* Get the list of all applications */
	app_list = gnome_vfs_mime_get_all_applications (mime_type);
	if (app_list != NULL) {
		for (list_element = app_list; list_element != NULL; list_element = list_element->next) {
			application = list_element->data;

			/* Create list item */
			list_item = create_application_list_item (application->id, application->name,
								  mime_type,
								  gnome_vfs_application_is_user_owned_application (application),
								  short_list);

			insert_item (GTK_LIST (list), list_item, -1);			
			
			}

		gnome_vfs_mime_application_list_free (app_list);
		
	}
	
	gnome_vfs_mime_application_list_free (short_list);
}


static gboolean 
component_is_in_list (const char *search_name, GList *component_list)
{
	GList *list_element;
	Bonobo_ServerInfo *info;
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
	Bonobo_ServerInfo *info;
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


 typedef struct {
 	GtkWidget *add_button;
 	GtkWidget *edit_button;
 	GtkWidget *delete_button;
 } ButtonHolder;


static void
check_button_status (GtkList *list, GtkWidget *widget, ButtonHolder *button_holder)
{
	int length = g_list_length (list->children);

	if (length == 0) {
		gtk_widget_set_sensitive (button_holder->delete_button, FALSE);
		gtk_widget_set_sensitive (button_holder->edit_button, FALSE);
	} else {
		gtk_widget_set_sensitive (button_holder->edit_button, TRUE);
	}
}

static void
update_delete_button (GtkList *list, GtkWidget *widget, ButtonHolder *button_holder)
{
	gtk_widget_set_sensitive (button_holder->delete_button,
				  GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (widget), "user_owned")));
}


/*
 *  initialize_edit_applications_dialog
 *  
 *  Set up dialog for default application list editing
 */
 
static void
initialize_edit_applications_dialog (const char *mime_type)
{
	GtkWidget *main_vbox, *hbox;
	GtkWidget *scroller, *label;
	GtkWidget *button, *list;
	char *label_text;
	ButtonHolder *button_holder;
	
	edit_application_details = g_new0 (edit_dialog_details, 1);

	edit_application_details->window = gnome_dialog_new (_("Edit Applications List"),
					     GNOME_STOCK_BUTTON_OK,
					     NULL,
					     NULL);

	/* FIXME: dialog should be parented on Control Center window */

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
	label_text = g_strdup_printf (_("Select applications to appear in menu for MIME type \"%s\""), mime_type);
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

	list = gtk_list_new ();
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scroller), list);
	gtk_list_set_selection_mode (GTK_LIST (list), GTK_SELECTION_BROWSE);

	/* Add edit buttons */	
	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (main_vbox), hbox, FALSE, FALSE, 0);	

	button_holder = g_new (ButtonHolder, 1);

	button = gtk_button_new_with_label (_("Add Application..."));	
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	gtk_object_set_data_full (GTK_OBJECT (button), "mime_type", g_strdup (mime_type), g_free);
	gtk_signal_connect (GTK_OBJECT (button), "clicked", show_new_application_window, list);
	gtk_widget_set_sensitive (GTK_WIDGET (button), TRUE);
	button_holder->add_button = button;
	
	button = gtk_button_new_with_label (_("Edit Application..."));	
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);	
	gtk_signal_connect (GTK_OBJECT (button), "clicked", show_edit_application_window, list);
	gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
	button_holder->edit_button = button;
	
	button = gtk_button_new_with_label (_("Delete Application"));
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (button), "clicked", delete_selected_application, list);
	gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
	button_holder->delete_button = button;

	/* Watch container so we can update buttons */
	gtk_signal_connect (GTK_OBJECT (list), "add", check_button_status, button_holder);
	gtk_signal_connect_full (GTK_OBJECT (list), "remove", check_button_status, NULL, button_holder,
			    	 g_free, FALSE, FALSE);			    	 
	gtk_signal_connect (GTK_OBJECT (list), "select_child", update_delete_button, button_holder);
			    	 
	gtk_widget_show_all (main_vbox);

	populate_default_applications_list (list, mime_type);
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
					     NULL,
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
	label_text = g_strdup_printf (_("Select views to appear in menu for MIME type \"%s\""), mime_type);
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

	/* FIXME: This is a modal dialog with no Cancel button, so the close box
	 * has to do the same thing as the OK button, which is pretty darn confusing.
	 * It would be better to make it modeless someday.
	 */
	gnome_dialog_run_and_close (GNOME_DIALOG (edit_application_details->window));
	nautilus_mime_type_capplet_update_application_info (mime_type);
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

	/* FIXME: This is a modal dialog with no Cancel button, so the close box
	 * has to do the same thing as the OK button, which is pretty darn confusing.
	 * It would be better to make it modeless someday.
	 */
	gnome_dialog_run_and_close (GNOME_DIALOG (edit_component_details->window));

	nautilus_mime_type_capplet_update_viewer_info (mime_type);
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
name_from_oaf_server_info (Bonobo_ServerInfo *server)
{
        const char *view_as_name;       
	char *display_name;
        GSList *langs;

        display_name = NULL;

        langs = get_lang_list ();
        view_as_name = bonobo_server_info_prop_lookup (server, "nautilus:view_as_name", langs);
		
        if (view_as_name == NULL) {
                view_as_name = bonobo_server_info_prop_lookup (server, "name", langs);
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


static void
find_message_label_callback (GtkWidget *widget, gpointer callback_data)
{
	find_message_label (widget, callback_data);
}

static void
find_message_label (GtkWidget *widget, const char *message)
{
	char *text;

	/* Turn on the flag if we find a label with the message
	 * in it.
	 */
	if (GTK_IS_LABEL (widget)) {
		gtk_label_get (GTK_LABEL (widget), &text);
		if (strcmp (text, message) == 0) {
			gtk_object_set_data (GTK_OBJECT (gtk_widget_get_toplevel (widget)),
					     "message label", widget);
		}
	}

	/* Recurse for children. */
	if (GTK_IS_CONTAINER (widget)) {
		gtk_container_foreach (GTK_CONTAINER (widget),
				       find_message_label_callback,
				       (char *) message);
	}
}

static GnomeDialog *
show_message_box (const char *message,
		  const char *dialog_title,
		  const char *type,
		  const char *button_one,
		  const char *button_two,
		  GtkWindow *parent)
{  
	GtkWidget *box;
	GtkLabel *message_label;

	g_assert (dialog_title != NULL);

	box = gnome_message_box_new (message, type, button_one, button_two, NULL);
	gtk_window_set_title (GTK_WINDOW (box), dialog_title);
	
	/* A bit of a hack. We want to use gnome_message_box_new,
	 * but we want the message to be wrapped. So, we search
	 * for the label with this message so we can mark it.
	 */
	find_message_label (box, message);
	message_label = GTK_LABEL (gtk_object_get_data (GTK_OBJECT (box), "message label"));
	gtk_label_set_line_wrap (message_label, TRUE);

	if (parent != NULL) {
		gnome_dialog_set_parent (GNOME_DIALOG (box), parent);
	}
	gtk_widget_show (box);
	return GNOME_DIALOG (box);
}

static void
display_upper_case_dialog (void)
{
	char *message;
	GnomeDialog *dialog;

	message = _("The MIME type entered contained upper case characters. "
		    "Upper case characters were changed to lower case for you.");
	
	dialog = show_message_box (message, _("Add New MIME Type"), 
				   GNOME_MESSAGE_BOX_INFO, GNOME_STOCK_BUTTON_OK, 
				   NULL, NULL);	

	gnome_dialog_run_and_close (dialog);
}

/* Do some basic validation of the text entry and enable the OK button if the text is
 * determined to be a valid string.
 */
static void
validate_text_and_update_button (GtkEntry *entry, gpointer data)
{
	char *text, *token;
	gboolean sensitize;
	
	sensitize = TRUE;
	
	text = gtk_entry_get_text (entry);
	if (text == NULL) {
		sensitize = FALSE;
	} else {	
		token = strtok (text, " ");
		if (token == NULL || strlen (token) <= 0) {
			/* Entered text is invalid as best as we can detect. */
			sensitize = FALSE;
		}
	}
	
	gtk_widget_set_sensitive (GTK_WIDGET (data), sensitize);
}

char *
nautilus_mime_type_capplet_show_new_mime_window (void)
{
	GtkWidget *dialog;
        GtkWidget *mime_entry;
	GtkWidget *label;
	GtkWidget *desc_entry;
	const char *description;
	char *mime_type, *tmp_str, c;
	gboolean upper_case_alert;
	
	mime_type = NULL;
	upper_case_alert = FALSE;
	
        dialog = gnome_dialog_new (_("Add MIME Type"), GNOME_STOCK_BUTTON_OK, 
				   GNOME_STOCK_BUTTON_CANCEL, NULL);
	gnome_dialog_set_default (GNOME_DIALOG (dialog), 1);

	label = gtk_label_new (_("New MIME type (e.g. image/x-thumper):"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label, TRUE, TRUE, 0);

	mime_entry = gtk_entry_new ();
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), mime_entry, TRUE, TRUE, 0);
	
       	label = gtk_label_new (_("Description (e.g. Thumper image):"));
	gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), label, TRUE, TRUE, 0);

	desc_entry = gtk_entry_new ();
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), desc_entry, TRUE, TRUE, 0);
	
	/* Set up text entry validation signal */
	gtk_signal_connect (GTK_OBJECT (mime_entry), "changed", 
			    GTK_SIGNAL_FUNC (validate_text_and_update_button), GNOME_DIALOG (dialog)->buttons->data);
	
	/* Set initial OK button state to desensitized */
	gtk_widget_set_sensitive (GTK_WIDGET (GNOME_DIALOG (dialog)->buttons->data), FALSE);
	
	/* Set focus to text entry widget */
	gtk_widget_grab_focus (mime_entry);

        gtk_widget_show_all (GNOME_DIALOG (dialog)->vbox);

        if (gnome_dialog_run (GNOME_DIALOG (dialog)) == GNOME_OK) {
		mime_type = g_strdup (gtk_entry_get_text (GTK_ENTRY (mime_entry)));
		g_assert (mime_type != NULL);
								
		/* Handle illegal mime types as best we can */
		for (tmp_str = mime_type; (c = *tmp_str) != '\0'; tmp_str++) {
			if (isascii ((guchar) c) && isupper ((guchar) c)) {
				*tmp_str = tolower (c);
				upper_case_alert = TRUE;
			}
		}
		
		description = gtk_entry_get_text (GTK_ENTRY (desc_entry));
		
		/* Add new mime type here */
		if (strlen (mime_type) > 3) {
			/* This call creates the key */
			gnome_vfs_mime_set_registered_type_key (mime_type, 
						  "description", 
						  description);
						  
			/* Ths call sets the user information */
			gnome_vfs_mime_set_value (mime_type, 
						  "description", 
						  description);
		}
        }

	gnome_dialog_close (GNOME_DIALOG (dialog));

	if (upper_case_alert) {
		display_upper_case_dialog ();
	}
	
        return mime_type;
}

static void
add_extension_clicked (GtkWidget *widget, gpointer data)
{
	GtkList *extension_list;
	char *new_extension;
	GtkWidget *new_list_item;
	GList *items_list;

	g_assert (GTK_IS_LIST (data));

	extension_list = GTK_LIST (data);

	new_extension = nautilus_mime_type_capplet_show_new_extension_window ();
	
	/* Filter out bogus extensions */
	if (new_extension == NULL || strlen (new_extension) <= 0 || new_extension[0] == ' ') {
		g_free (new_extension);
		return;
	}
	
	new_list_item = gtk_list_item_new_with_label (new_extension);
	gtk_widget_show (new_list_item);

	items_list = g_list_append (NULL, new_list_item);
	gtk_list_append_items (GTK_LIST (extension_list), items_list);
	g_free (new_extension);
	/* GtkList takes ownership of the List we append. DO NOT free it. */
}

static void
remove_extension_clicked (GtkWidget *widget, gpointer data)
{
	GtkList *list;
	GList *selection_copy, *temp;
	
	g_assert (GTK_IS_LIST (data));

	list = GTK_LIST (data);

	/* this is so fucking crapy !!! */
	/* you have to make a copy of the selection list before 
	   passing it to remove_items because before removing the
	   widget from the List, it modifies the list.
	   So, when you remove it, the list is not valid anymore */
	selection_copy = NULL; 
	for (temp = list->selection; temp != NULL; temp = temp->next) {
		selection_copy = g_list_prepend (selection_copy, temp->data);
	}

	if (list->selection != NULL) {
		gtk_list_remove_items (GTK_LIST (list), selection_copy);
		gtk_list_select_item (GTK_LIST (list), 0);
	}

	g_list_free (selection_copy);
}

static void
extension_list_selected (GtkWidget *list, GtkWidget *child, gpointer data)
{
	GtkWidget *button;

	g_assert (GTK_IS_BUTTON (data));

	button = GTK_WIDGET (data);

	gtk_widget_set_sensitive (GTK_WIDGET (button), TRUE);
}

static void
extension_list_deselected (GtkWidget *list, GtkWidget *child, gpointer data)
{
	GtkWidget *button;

	g_assert (GTK_IS_BUTTON (data));

	button = GTK_WIDGET (data);

	gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);
}

static char *
get_extensions_from_gtk_list (GtkList *list) 
{
	GList *temp;
	GtkLabel *label;
	char *extension, *extensions, *temp_text;
	
	extensions = NULL;
	for (temp = list->children; temp != NULL; temp = temp->next) {
		label = GTK_LABEL (GTK_BIN (temp->data)->child);
		gtk_label_get (GTK_LABEL (label), &extension);
		temp_text = g_strconcat (extension, " ", extensions, NULL);
		g_free (extensions);
		extensions = temp_text;
	}

	return extensions;
}

char *
nautilus_mime_type_capplet_show_change_extension_window (const char *mime_type, gboolean *new_list)
{
	GtkWidget *dialog;
	GtkWidget *hbox;
	GtkWidget *button;
	GtkWidget *list;
	char *extensions_list_str;

	*new_list = FALSE;
	
        dialog = gnome_dialog_new (_("File Extensions "), 
				   GNOME_STOCK_BUTTON_OK, 
				   GNOME_STOCK_BUTTON_CANCEL, 
				   NULL);
	gnome_dialog_set_default (GNOME_DIALOG (dialog), 1);
	gnome_dialog_set_close (GNOME_DIALOG (dialog), FALSE);

	hbox = gtk_hbox_new (FALSE, GNOME_PAD_BIG);
        gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), hbox, FALSE, FALSE, 0);


	list = gtk_list_new ();

	/* the right buttons */
	{
		GtkWidget *vbox;

		vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
		gtk_box_pack_end (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

		button = gtk_button_new_with_label (_("Add..."));
		gtk_signal_connect (GTK_OBJECT (button), "clicked", 
				    add_extension_clicked, list);
		gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

		button = gtk_button_new_with_label (_("    Remove    "));
		gtk_widget_set_sensitive (button, FALSE);
		gtk_signal_connect (GTK_OBJECT (button), "clicked", 
				    remove_extension_clicked, list);
		gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

		gtk_box_pack_start (GTK_BOX (vbox), gtk_label_new (""), FALSE, FALSE, 0);
	}

	/* The left list */
	{
		GtkWidget *viewport;
		GList *extensions_list, *widget_list, *temp;

		viewport = gtk_viewport_new (NULL, NULL);
		gtk_box_pack_start (GTK_BOX (hbox), viewport, TRUE, TRUE, 0);

		gtk_signal_connect (GTK_OBJECT (list), "select_child", 
				    extension_list_selected, button);
		gtk_signal_connect (GTK_OBJECT (list), "unselect_child", 
				    extension_list_deselected, button);
		gtk_list_set_selection_mode (GTK_LIST (list), GTK_SELECTION_SINGLE);
		gtk_container_add (GTK_CONTAINER (viewport), list);


		extensions_list = gnome_vfs_mime_get_extensions_list (mime_type);
		if (extensions_list != NULL) {
			widget_list = NULL;
			for (temp = extensions_list; temp != NULL; temp = temp->next) {
				widget_list = g_list_append (widget_list, 
							     gtk_list_item_new_with_label ((char *) temp->data));
			}
			gtk_list_append_items (GTK_LIST (list), widget_list);
		}

		gtk_list_select_item (GTK_LIST (list), 0);
	}



        gtk_widget_show_all (GNOME_DIALOG (dialog)->vbox);

	extensions_list_str = NULL;
        if (gnome_dialog_run (GNOME_DIALOG (dialog)) == GNOME_OK) {
		*new_list = TRUE;
		extensions_list_str = get_extensions_from_gtk_list (GTK_LIST (list));
		if (extensions_list_str == NULL) {
			extensions_list_str = g_strdup ("");				
		}
	}
	if (extensions_list_str == NULL) {
		extensions_list_str = g_strdup ("");
	}
	gnome_dialog_close (GNOME_DIALOG (dialog));


	return extensions_list_str;
}


char *
nautilus_mime_type_capplet_show_new_extension_window (void)
{
        GtkWidget *mime_entry;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *dialog;
	char *new_extension;
	
        dialog = gnome_dialog_new (_("Add New Extension"), GNOME_STOCK_BUTTON_OK, 
				   GNOME_STOCK_BUTTON_CANCEL, NULL);
	gnome_dialog_set_default (GNOME_DIALOG (dialog), 0);
	gnome_dialog_set_close (GNOME_DIALOG (dialog), FALSE);
	label = gtk_label_new (_("Type in the extensions for this mime-type (without dot).\n"
				 "You can enter several extensions seperated by a space,\n"
				 "for example: html htm"));
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

	/* Set focus to text entry widget */
	gtk_widget_grab_focus (mime_entry);
	
        if (gnome_dialog_run (GNOME_DIALOG (dialog)) == GNOME_OK) {
		new_extension = g_strdup (gtk_entry_get_text (GTK_ENTRY (mime_entry)));
	} else {
		new_extension = g_strdup ("");
	}

	gnome_dialog_close (GNOME_DIALOG (dialog));
	
	return new_extension;
}

/* add_or_update_application
 *
 * Create or update a GnomeVFSMimeApplication and register
 * it with the mime database.
 */
static char *
add_or_update_application (GtkWidget *list, const char *name, const char *command,
		     	   gboolean multiple, gboolean expects_uris, 
			   gboolean update)
{
	GnomeVFSMimeApplication app, *original;
	const char *mime_type;
	uuid_t app_uuid;
	char app_uuid_string[100];
	
	/* Check for empty strings.  Command can be empty. */
	if (name[0] == '\0') {
		return NULL;
	}

	mime_type = nautilus_mime_type_capplet_get_selected_item_mime_type ();
	g_assert (mime_type != NULL);

	/* It's ok to cast, we don't modify the application
	 * structure and thus the name/command, this should really
	 * use the application registry explicitly */
	 	
	/* Generate unique application id */
	uuid_generate(app_uuid);
	uuid_unparse(app_uuid, app_uuid_string);	
	app.id = app_uuid_string;	
	app.name = (char *)name;
	app.command = (char *)command;
	app.can_open_multiple_files = multiple;
	app.expects_uris = expects_uris;
	/* FIXME: We should be getting this information */
	app.supported_uri_schemes = NULL;
	app.requires_terminal = FALSE;

	if (update) {
		original = gnome_vfs_mime_application_new_from_id (app.id);
		if (original == NULL) {
			const char *original_id;
			GList *selection;
			GtkListItem *item;
			int position;
			
			/* If there isn't a selection we cannot allow an edit */
			selection = GTK_LIST (list)->selection;
			if (selection == NULL || g_list_length (selection) <= 0) {
				return NULL;
			}

			/* Get application id and info */
			item = GTK_LIST_ITEM (selection->data);
			if (item == NULL) {
				return NULL;
			}

			original_id = gtk_object_get_data (GTK_OBJECT (item), "application_id");
			if (original_id == NULL) {
				return NULL;
			}
			
			/* Remove original application data */
			gnome_vfs_application_registry_remove_mime_type (original_id, mime_type);		
			gnome_vfs_application_registry_sync ();
			gnome_vfs_mime_remove_application_from_short_list (mime_type, original_id);
			
			/* Remove widget from list */
			position = gtk_list_child_position (GTK_LIST (list), GTK_WIDGET (item));
			gtk_container_remove (GTK_CONTAINER (list), GTK_WIDGET (item));

			/* Add new widget and restore position */
			add_item_to_application_list (list, original_id, name, mime_type,
						      gnome_vfs_application_is_user_owned_application (original),
						      position);
		}
	}
	
	gnome_vfs_application_registry_save_mime_application (&app);
	gnome_vfs_application_registry_add_mime_type (app.id, mime_type);
	gnome_vfs_application_registry_sync ();

	gnome_vfs_mime_add_application_to_short_list (mime_type, app.id);
	
	return g_strdup (app.id);
}

static void
add_item_to_application_list (GtkWidget *list, const char *id, const char *name, const char *mime_type,
			      gboolean user_owned, int position)
{
	GtkListItem *list_item;
	GList *short_list;

	short_list = gnome_vfs_mime_get_short_list_applications (mime_type);
	list_item = create_application_list_item (id, name, mime_type, user_owned,  short_list);
	gnome_vfs_mime_application_list_free (short_list);

	insert_item (GTK_LIST (list), list_item, position);
	gtk_list_select_child (GTK_LIST (list), GTK_WIDGET (list_item));	
}

static gboolean
handle_invalid_application_input (GtkWindow *parent_window, const char *name, const char *command)
{
	char *message;
	char *stripped_name;
	GnomeDialog *error_dialog;
	gboolean error_in_name;

	message = NULL;
	error_in_name = FALSE;

	stripped_name = g_strstrip (g_strdup (name));
	 
	if (strlen (stripped_name) == 0) {
		message = g_strdup (_("You must enter a name."));
		error_in_name = TRUE;
	} else if (strlen (command) == 0) {
		message = g_strdup (_("You must enter a command."));
	} else if (!gnome_vfs_is_executable_command_string (command)) {
		if (command[0] == '/') {
			/* FIXME: Should strip parameters off before using in this message. */
			/* FIXME: Should use separate messages for doesn't exist/isn't executable. */
			/* Both of these FIXMEs would need to handle quoting to work correctly, 
			 * since otherwise a space might be part of path or separator before parameters.
			 */
			/* FIXME: Should use some line-wrapping technology a la nautilus-stock-dialogs.c */
			message = g_strdup_printf
				(_("\"%s\" does not exist or is not executable.\n"
				   "Check your spelling and make sure you have\n"
				   "the right permissions to execute this file."), command);
		} else {
			/* FIXME: Should strip parameters off before using in this message */
			message = g_strdup_printf
				(_("The command \"%s\" cannot be found.\n"
				   "You must use a command that can work from any command line."), command);
		}
	}

	g_free (stripped_name);
	
	if (message != NULL) {
		error_dialog = GNOME_DIALOG (gnome_error_dialog_parented (message,
								          parent_window));
		gtk_window_set_title (GTK_WINDOW (error_dialog), 
				      error_in_name 
				      	? _("Bad Application Name")
				      	: _("Bad Application Command"));
		
		gnome_dialog_run (error_dialog);
		g_free (message);
		
		return TRUE;
	}

	return FALSE;
}

static void
run_edit_or_new_application_dialog (const char *mime_type, GtkWidget *list, GnomeVFSMimeApplication *application)
{
	GtkWidget *dialog;
	GtkWidget *app_entry, *command_entry;
	GtkWidget *label;
	GtkWidget *behavior_frame, *frame_vbox;
	GtkWidget *multiple_check_box, *uri_check_box;
	GtkWidget *table;	
	gboolean initial_toggle_state;
	const char *name;
	const char *command;
	int dialog_result;
	gboolean entry_validated;
	char *invalid_entry_message, *app_id;

	g_assert (mime_type != NULL || application != NULL);
	g_assert (GTK_IS_WIDGET (list));

	dialog = gnome_dialog_new (
		application == NULL
			? _("Add Application")
			: _("Edit Application"), 
		GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);	

	/* FIXME: Dialog should be parented on Edit Applications dialog */
	
	/* Create table */
	table = gtk_table_new (4, 2, FALSE);
	gtk_container_add (GTK_CONTAINER (GNOME_DIALOG (dialog)->vbox), table);	
	gtk_table_set_row_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);
	gtk_table_set_col_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);

	/* Application Name label and entry */
	label = gtk_label_new (_("Application Name:"));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);

	app_entry = gtk_entry_new ();
	gtk_table_attach_defaults ( GTK_TABLE (table), app_entry, 1, 2, 0, 1);
	if (application != NULL) {
		gtk_entry_set_text (GTK_ENTRY (app_entry), application->name);
	}

	/* Application Command label and entry */
	label = gtk_label_new (_("Application Command:"));
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);

	command_entry = gtk_entry_new ();
	gtk_table_attach_defaults (GTK_TABLE (table), command_entry, 1, 2, 1, 2);
	if (application != NULL) {
		gtk_entry_set_text (GTK_ENTRY (command_entry), application->command);
	}
	
	/* Open Behavior frame */
	behavior_frame = gtk_frame_new (_("Open Behavior"));
	gtk_table_attach_defaults ( GTK_TABLE (table), behavior_frame, 0, 2, 2, 3);
	
	frame_vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (behavior_frame), frame_vbox);

	multiple_check_box = gtk_check_button_new_with_label (_("Can open multiple files"));
	gtk_box_pack_start (GTK_BOX (frame_vbox), multiple_check_box, FALSE, FALSE, 0);
	initial_toggle_state = application == NULL
		? FALSE
		: application->can_open_multiple_files;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (multiple_check_box), initial_toggle_state);

	/* FIXME bugzilla.eazel.com 6066: This needs to be three options now: "yes", "no", and "use uris for non-file locations" */
	uri_check_box = gtk_check_button_new_with_label (_("Can open from URI"));
	gtk_box_pack_start (GTK_BOX (frame_vbox), uri_check_box, FALSE, FALSE, 0);
	initial_toggle_state = application == NULL
		? FALSE
		: application->expects_uris;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (uri_check_box), initial_toggle_state);


	gtk_widget_show_all (GNOME_DIALOG (dialog)->vbox);

	/* Set focus to text entry widget */
	gtk_widget_grab_focus (app_entry);

	do {
		dialog_result = gnome_dialog_run (GNOME_DIALOG (dialog));
		entry_validated = FALSE;

		if (dialog_result == GNOME_OK) {
			name = gtk_entry_get_text (GTK_ENTRY (app_entry));
			command = gtk_entry_get_text (GTK_ENTRY (command_entry));

			invalid_entry_message = NULL;

			if (!handle_invalid_application_input (GTK_WINDOW (dialog), name, command)) {
				entry_validated = TRUE;
				app_id = add_or_update_application (list,
							    	    name,
						     	   	    command,
		        			     	   	    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (multiple_check_box)),
		        			     	   	    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (uri_check_box)),
		        			     	   	    application != NULL);
		        	if (application == NULL && app_id != NULL) {
					add_item_to_application_list (list, app_id, name, mime_type, TRUE, -1);					
		        	}
		        	g_free (app_id);
			}
		}
	} while (dialog_result == GNOME_OK && !entry_validated);
	
	/* FIXME: Close box is treated like Cancel, which loses user changes silently.
	 * Would be better to either do nothing at all (force use of OK or Cancel) or
	 * even put up a little dialog telling them they have to use OK or Cancel.
	 * Too bad we can't prevent the close box from appearing. Window Managers suck.
	 */
	if (dialog_result >= 0) {
		gnome_dialog_close (GNOME_DIALOG (dialog));
	}
}
	
static void
show_new_application_window (GtkWidget *button, GtkWidget *list)
{
	run_edit_or_new_application_dialog (gtk_object_get_data (GTK_OBJECT (button), "mime_type"), list, NULL);
}

static void
show_edit_application_window (GtkWidget *button, GtkWidget *list)
{
	GList *selection;
	const char *id;
	GnomeVFSMimeApplication *application;
	GtkListItem *item;
	
	/* If there isn't a selection we cannot allow an edit */
	selection = GTK_LIST (list)->selection;
	if (selection == NULL || g_list_length (selection) <= 0) {
		return;
	}

	/* Get application id and info */
	item = GTK_LIST_ITEM (selection->data);
	if (item == NULL) {
		return;
	}

	id = gtk_object_get_data (GTK_OBJECT (item), "application_id");
	if (id == NULL) {
		return;
	}

	application = gnome_vfs_mime_application_new_from_id (id);
	if (application == NULL) {
		return;
	}

	run_edit_or_new_application_dialog (NULL, list, application);

       	gnome_vfs_mime_application_free (application);
}

static void
delete_selected_application (GtkWidget *button, GtkWidget *list)
{
	GtkListItem *item;
	const char *mime_type, *id;
	GList *selection;
	
	/* Get selected list item */
	selection = GTK_LIST (list)->selection;
	if (selection == NULL) {
		return;
	}
	
	item = GTK_LIST_ITEM (selection->data);
	if (item == NULL) {
		return;
	}
	
	g_return_if_fail (GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (item), "user_owned")));
	
	id = gtk_object_get_data (GTK_OBJECT (item), "application_id");
	mime_type = gtk_object_get_data (GTK_OBJECT (item), "mime_type");

	/* Remove application if it is user owned */
	if (id != NULL && mime_type != NULL) {
		gnome_vfs_application_registry_remove_mime_type (id, mime_type);
		gnome_vfs_application_registry_sync ();
		gnome_vfs_mime_remove_application_from_short_list (mime_type, id);
	}
	
	/* Remove widget from list */
	gtk_container_remove (GTK_CONTAINER (list), GTK_WIDGET (item));
}
