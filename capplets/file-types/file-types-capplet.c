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
#include <math.h>
#include <sys/types.h>
#include <regex.h>
#include <string.h>

#include <capplet-widget.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkprivate.h>
#include <gnome.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include "file-types-capplet-dialogs.h"
#include "file-types-icon-entry.h"

#include "file-types-capplet.h"

#define DEFAULT_REGULAR_ICON "nautilus/i-regular-24.png"
#define DEFAULT_ACTION_ICON "nautilus/i-executable.png"

#define MAX_ICON_WIDTH_IN_LIST	18
#define MAX_ICON_HEIGHT_IN_LIST	18

enum {
	COLUMN_DESCRIPTION = 0,
	COLUMN_MIME_TYPE,
	COLUMN_EXTENSION,
	COLUMN_ACTION,
	TOTAL_COLUMNS
};

/* Local Prototypes */
static void	 init_mime_capplet 	  		(const char 	*scroll_to_mime_type);
static void 	 populate_application_menu 		(GtkWidget 	*menu, 	 
						 	 const char 	*mime_string);
static void 	 populate_viewer_menu			(GtkWidget 	*menu, 	 
						 	 const char 	*mime_string);
static void	 revert_mime_clicked       		(GtkWidget 	*widget, 
						 	 gpointer   	data);
static void	 delete_mime_clicked       		(GtkWidget 	*widget, 
						 	 gpointer   	data);
static void	 add_mime_clicked 	  		(GtkWidget 	*widget, 
						 	 gpointer   	data);
static void	 edit_default_clicked 			(GtkWidget 	*widget, 	
						 	 gpointer   	data);
static GtkWidget *create_mime_list_and_scroller 	(void);
static void 	 ok_callback 		  		(void);
static void 	 help_callback 		  		(void);
static void	 gtk_widget_make_bold 			(GtkWidget 	*widget);
static GdkFont 	 *gdk_font_get_bold 			(const GdkFont  *plain_font);
static void	 gtk_widget_set_font 			(GtkWidget 	*widget, 
						 	 GdkFont 	*font);
static void	 gtk_style_set_font 			(GtkStyle 	*style, 
						 	 GdkFont 	*font);
static GdkPixbuf *capplet_gdk_pixbuf_scale_to_fit 	(GdkPixbuf 	*pixbuf, 
							 int 		max_width, 
							 int 		max_height);
static void	 update_mime_list_action 		(const char 	*mime_string);
static void      populate_mime_list                     (GList          *type_list, 
							 GtkCList       *clist);
static GdkPixbuf *capplet_get_icon_pixbuf               (const char     *mime_string, 
							 gboolean        is_executable);


/* FIXME: Using global variables here is yucky */
GtkWidget *capplet;
GtkWidget *delete_button;
GtkWidget *remove_button;
GtkWidget *add_button;
GtkWidget *icon_entry, *extension_list, *mime_list;
GtkWidget *default_menu;
GtkWidget *application_button, *viewer_button;
GtkLabel  *mime_label;
GtkWidget *description_entry;
gboolean   description_has_changed;
gboolean sort_column_clicked [TOTAL_COLUMNS];

/*
 *  main
 *
 *  Display capplet
 */

#define MATHIEU_DEBUG

#ifdef MATHIEU_DEBUG
#include <signal.h>

static void
nautilus_stop_in_debugger (void)
{
	void (* saved_handler) (int);

	saved_handler = signal (SIGINT, SIG_IGN);
	raise (SIGINT);
	signal (SIGINT, saved_handler);
}

static void
nautilus_stop_after_default_log_handler (const char *domain,
					 GLogLevelFlags level,
					 const char *message,
					 gpointer data)
{
	g_log_default_handler (domain, level, message, data);
	nautilus_stop_in_debugger ();
}

static void
nautilus_set_stop_after_default_log_handler (const char *domain)
{
	g_log_set_handler (domain, G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING,
			   nautilus_stop_after_default_log_handler, NULL);
}

static void
nautilus_make_warnings_and_criticals_stop_in_debugger (const char *first_domain, ...)
{
	va_list domains;
	const char *domain;

	nautilus_set_stop_after_default_log_handler (first_domain);

	va_start (domains, first_domain);

	for (;;) {
		domain = va_arg (domains, const char *);
		if (domain == NULL) {
			break;
		}
		nautilus_set_stop_after_default_log_handler (domain);
	}

	va_end (domains);
}
#endif /* MATHIEU_DEBUG */

int
main (int argc, char **argv)
{
        int init_results;
	char *mime_type;
	
	mime_type = NULL;
	
	/* */
	if (argc >= 1) {
		mime_type = g_strdup (argv[1]);
	}
		
        bindtextdomain (PACKAGE, GNOMELOCALEDIR);
        textdomain (PACKAGE);
	
	
        init_results = gnome_capplet_init ("mime-type-capplet", VERSION, argc, argv, NULL, 0, NULL);

	if (init_results < 0) {
                exit (0);
	}

	gnome_vfs_init ();

#ifdef MATHIEU_DEBUG
	nautilus_make_warnings_and_criticals_stop_in_debugger (G_LOG_DOMAIN, g_log_domain_glib,
							       "Bonobo",
							       "Gdk",
							       "GnomeUI",
							       "GnomeVFS",
							       "GnomeVFS-CORBA",
							       "GnomeVFS-pthread",
							       "Gtk",
							       "Nautilus",
							       "Nautilus-Authenticate",
							       "Nautilus-Tree",
							       "ORBit",
							       NULL);
							       
#endif /* MATHIEU_DEBUG */
	if (init_results == 0) {
		init_mime_capplet (mime_type);
	        capplet_gtk_main ();
	}
        return 0;
}

static void
ok_callback ()
{
}

static void
help_callback ()
{
    GnomeHelpMenuEntry help_entry= {"control-center",
    "filetypes.html"};
    gnome_help_display (NULL, &help_entry);
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

	extensions = gnome_vfs_mime_get_extensions_list (mime_type);
	if (extensions == NULL) {
		return;
	}

	for (element = extensions; element != NULL; element = element->next) {
		extension[0] = (char *)element->data;
		if (strlen (element->data) > 0) {
			row = gtk_clist_append (list, extension);
			
			/* Set to deletable */
			gtk_clist_set_row_data (list, row, GINT_TO_POINTER (FALSE));
		}
	}

	gnome_vfs_mime_extensions_list_free (extensions);	
	
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

	/* Copy only contiguous part of string.  No spaces allowed. */	
	search_string = g_strdup (extension);
	token = strtok (search_string, " ");

	if (token == NULL) {		
		title[0] = g_strdup (extension);
	} else if (strlen (token) <= 0) {
		return;
	} else {
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
mime_list_selected_row_callback (GtkWidget *widget, gint row, gint column, GdkEvent *event, gpointer data)
{
        const char *mime_type;

        mime_type = (const char *) gtk_clist_get_row_data (GTK_CLIST (widget),row);

	/* Update info on selection */
        nautilus_mime_type_capplet_update_info (mime_type);
}

static void
application_button_toggled (GtkToggleButton *button, gpointer user_data)
{
	const char *mime_type;
	
	if (gtk_toggle_button_get_active (button)) {

		mime_type = nautilus_mime_type_capplet_get_selected_item_mime_type ();
		
		gnome_vfs_mime_set_default_action_type (mime_type, GNOME_VFS_MIME_ACTION_TYPE_APPLICATION);

		/* Populate menu with application items */
		populate_application_menu (default_menu, mime_type);

		/* Update mime list */
		update_mime_list_action (mime_type);
	}
}

static void
viewer_button_toggled (GtkToggleButton *button, gpointer user_data)
{
	const char *mime_type;
	
	if (gtk_toggle_button_get_active (button)) {
		mime_type = nautilus_mime_type_capplet_get_selected_item_mime_type ();

		gnome_vfs_mime_set_default_action_type (mime_type, GNOME_VFS_MIME_ACTION_TYPE_COMPONENT);

		/* Populate menu with viewer items */
		populate_viewer_menu (default_menu, mime_type);

		/* Update mime list */
		update_mime_list_action (mime_type);
	}
}

static int
get_selected_row_number (void)
{
	gint row;

	if (GTK_CLIST (mime_list)->selection == NULL) {
		return -1;
	}

	row = GPOINTER_TO_INT ((GTK_CLIST (mime_list)->selection)->data);
	return row;
}
static const char *
get_selected_mime_type (void)
{
        gint row = 0;
        const char *mime_type;


	if (GTK_CLIST (mime_list)->selection == NULL) {
		return NULL;
	}

	row = get_selected_row_number ();
	if (row == -1) {
		return NULL;
	}

	mime_type = (const char *) gtk_clist_get_row_data (GTK_CLIST (mime_list), row);
	
	return mime_type;
}

static void
really_change_icon (gpointer user_data)
{
	NautilusMimeIconEntry *icon_entry;
	char *filename;
	const char *mime_type;

	g_assert (NAUTILUS_MIME_IS_ICON_ENTRY (user_data));

	mime_type = get_selected_mime_type ();
	if (mime_type == NULL) {
		return;
	}

	icon_entry = NAUTILUS_MIME_ICON_ENTRY (user_data);

	filename = nautilus_mime_type_icon_entry_get_relative_filename (icon_entry);
	if (filename == NULL) {
		filename = nautilus_mime_type_icon_entry_get_full_filename (icon_entry);
	}

	gnome_vfs_mime_set_icon (mime_type, filename);

	nautilus_mime_type_capplet_update_mime_list_icon_and_description (mime_type);
	nautilus_mime_type_capplet_update_info (mime_type);

	g_free (filename);
}

static void
icon_chosen_callback (GnomeIconList *gil, gint num, GdkEvent *event, gpointer user_data)
{
	NautilusMimeIconEntry *icon_entry;
	const gchar * icon;
	GnomeIconSelection * gis;
	GtkWidget *gtk_entry;

	g_return_if_fail (user_data != NULL);
	g_return_if_fail (NAUTILUS_MIME_IS_ICON_ENTRY (user_data));

	icon_entry = NAUTILUS_MIME_ICON_ENTRY (user_data);

	gis =  gtk_object_get_user_data (GTK_OBJECT(icon_entry));
	icon = gnome_icon_selection_get_icon(gis, TRUE);

	if (icon != NULL) {
		gtk_entry = nautilus_mime_type_icon_entry_gtk_entry(icon_entry);
		gtk_entry_set_text(GTK_ENTRY(gtk_entry),icon);
		
	}

	if(event && event->type == GDK_2BUTTON_PRESS && ((GdkEventButton *)event)->button == 1) {
		gnome_icon_selection_stop_loading(gis);
		really_change_icon (user_data);
		gtk_widget_hide(icon_entry->pick_dialog);
	}


}

static void
change_icon_clicked_cb_real (GnomeDialog *dialog, gint button_number, gpointer user_data)
{
	if (button_number == GNOME_OK) {
		really_change_icon (user_data);
	}
}

static void 
change_icon_clicked (GtkWidget *entry, gpointer user_data)
{
	GnomeDialog *dialog;
	GnomeIconSelection * gis;

	nautilus_mime_type_show_icon_selection (NAUTILUS_MIME_ICON_ENTRY (user_data));

	dialog = GNOME_DIALOG (NAUTILUS_MIME_ICON_ENTRY (user_data)->pick_dialog);

	gtk_signal_connect (GTK_OBJECT (dialog), "clicked", change_icon_clicked_cb_real, user_data);

	gis = gtk_object_get_user_data(GTK_OBJECT(user_data));
	gtk_signal_connect_after (GTK_OBJECT(GNOME_ICON_SELECTION(gis)->gil), 
				  "select_icon", icon_chosen_callback, user_data);

}

static void
update_extensions_list (const char *mime_type) 
{
	int row;
	char *pretty_string;

	row = get_selected_row_number ();

	pretty_string = gnome_vfs_mime_get_extensions_pretty_string (mime_type);
	if (pretty_string == NULL) {
		pretty_string = g_strdup (" ");
	}
	gtk_clist_set_text (GTK_CLIST (mime_list),
			    row, 2, pretty_string);

	g_free (pretty_string);

}

static void 
change_file_extensions_clicked (GtkWidget *widget, gpointer user_data)
{
	const char *mime_type;
	char *new_extensions;
	gboolean use_new_list;

	mime_type = get_selected_mime_type ();
	if (mime_type == NULL) {
		return;
	}

	new_extensions = nautilus_mime_type_capplet_show_change_extension_window (mime_type, &use_new_list);
	if (use_new_list) {
		gnome_vfs_mime_set_extensions_list (mime_type, new_extensions);
	}

	update_extensions_list (mime_type);
}

/* The following are copied from gtkclist.c and nautilusclist.c */ 
#define CELL_SPACING 1

/* gives the top pixel of the given row in context of
 * the clist's voffset */
#define ROW_TOP_YPIXEL(clist, row) (((clist)->row_height * (row)) + \
				    (((row) + 1) * CELL_SPACING) + \
				    (clist)->voffset)
static void
list_move_vertical (GtkCList *clist, gint row, gfloat align)
{
	gfloat value;

	g_return_if_fail (clist != NULL);

	if (!clist->vadjustment) {
		return;
	}

	value = (ROW_TOP_YPIXEL (clist, row) - clist->voffset -
		 align * (clist->clist_window_height - clist->row_height) +
		 (2 * align - 1) * CELL_SPACING);

	if (value + clist->vadjustment->page_size > clist->vadjustment->upper) {
		value = clist->vadjustment->upper - clist->vadjustment->page_size;
	}

	gtk_adjustment_set_value (clist->vadjustment, value);
}

static void
list_moveto (GtkCList *clist, gint row, gint column, gfloat row_align, gfloat col_align)
{
	g_return_if_fail (clist != NULL);

	if (row < -1 || row >= clist->rows) {
		return;
	}
	
	if (column < -1 || column >= clist->columns) {
		return;
	}

	row_align = CLAMP (row_align, 0, 1);
	col_align = CLAMP (col_align, 0, 1);

	/* adjust vertical scrollbar */
	if (clist->vadjustment && row >= 0) {
		list_move_vertical (clist, row, row_align);
	}
}

static void
list_reveal_row (GtkCList *clist, int row_index)
{
	g_return_if_fail (row_index >= 0 && row_index < clist->rows);
		
	if (ROW_TOP_YPIXEL (clist, row_index) + clist->row_height > clist->clist_window_height) {
		list_moveto (clist, row_index, -1, 1, 0);
     	} else if (ROW_TOP_YPIXEL (clist, row_index) < 0) {
		list_moveto (clist, row_index, -1, 0, 0);
     	}
}



static int
find_row_for_mime_type (const char *mime_type, GtkCList *mime_list)
{
	gboolean found_one;
	int index;
	const char *row_data;
	
	if (mime_type == NULL) {
		return -1;
	}
	
	found_one = FALSE;
	
	for (index = 0; index < mime_list->rows; index++) {
		row_data = gtk_clist_get_row_data (mime_list, index);
		if (row_data != NULL && strcmp (row_data, mime_type) == 0) {
			found_one = TRUE;
			break;	
		}		
	}
				
	if (found_one) {
		return index;
	}
	
	return -1;
}


static void
update_description_from_input (GtkEntry *entry)
{
	char *new_description;
	const char *mime_type;

	g_assert (GTK_IS_ENTRY (entry));
	g_assert ((gpointer)entry == (gpointer)description_entry);

	description_has_changed = FALSE;

	mime_type = get_selected_mime_type ();
	if (mime_type == NULL) {
		return;
	}

	new_description = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);
	gnome_vfs_mime_set_description (mime_type, new_description);
	nautilus_mime_type_capplet_update_mime_list_icon_and_description (mime_type);
	g_free (new_description);
}

static void
description_entry_activate (GtkEntry *entry, gpointer user_data)
{
	g_assert (GTK_IS_ENTRY (entry));
	g_assert ((gpointer)entry == (gpointer)description_entry);
	g_assert (user_data == NULL);
	
	if (description_has_changed) {
		update_description_from_input (entry);
	}
}

static void
description_entry_changed (GtkEntry *entry, gpointer user_data)
{
	g_assert (GTK_IS_ENTRY (entry));
	g_assert ((gpointer)entry == (gpointer)description_entry);
	g_assert (user_data == NULL);
	
	description_has_changed = TRUE;
}

static gboolean
description_entry_lost_focus (GtkEntry *entry,
			      GdkEventFocus *event,
			      gpointer user_data)
{
	g_assert (GTK_IS_ENTRY (entry));
	g_assert ((gpointer)entry == (gpointer)description_entry);
	g_assert (user_data == NULL);
	
	if (description_has_changed) {
		update_description_from_input (entry);
	}

	return FALSE;
}

static void
init_mime_capplet (const char *scroll_to_mime_type)
{
	GtkWidget *main_vbox;
        GtkWidget *vbox, *hbox, *left_vbox;
        GtkWidget *button;
        GtkWidget *mime_list_container;
        GtkWidget *frame;
        GtkWidget *table;
	int index, list_width, column_width, found_index;

	capplet = capplet_widget_new ();

	/* Main vertical box */                    
	main_vbox = gtk_vbox_new (FALSE, GNOME_PAD);
	gtk_container_set_border_width (GTK_CONTAINER (main_vbox), GNOME_PAD_SMALL);
        gtk_container_add (GTK_CONTAINER (capplet), main_vbox);

        /* Main horizontal box and mime list */                    
        hbox = gtk_hbox_new (FALSE, GNOME_PAD);
        gtk_box_pack_start (GTK_BOX (main_vbox), hbox, TRUE, TRUE, 0);
        mime_list_container = create_mime_list_and_scroller ();
        gtk_box_pack_start (GTK_BOX (hbox), mime_list_container, TRUE, TRUE, 0);         
	
	vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
        gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

	/* Create table */
	table = gtk_table_new (2, 2, FALSE);
	gtk_box_pack_start (GTK_BOX (main_vbox), table, FALSE, FALSE, 0);
	gtk_table_set_row_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);
	gtk_table_set_col_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);


	left_vbox = gtk_vbox_new (FALSE, 0);
	/* Set up top left area. */
	{
		GtkWidget *small_table;

		small_table =  gtk_table_new (1, 2, FALSE);
		gtk_table_set_col_spacings (GTK_TABLE (small_table), 7);

		gtk_table_attach ( GTK_TABLE (table), small_table, 0, 1, 0, 1,
				   (GtkAttachOptions) (GTK_FILL),
				   (GtkAttachOptions) (0), 0, 0);
				
		icon_entry = nautilus_mime_type_icon_entry_new ("mime_icon_entry", NULL);
		gtk_table_attach (GTK_TABLE (small_table), icon_entry, 0, 1, 0, 1,
				  (GtkAttachOptions) (GTK_FILL),
				  (GtkAttachOptions) (0), 0, 0);
				  
		vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
		gtk_table_attach (GTK_TABLE (small_table), vbox, 1, 2, 0, 1,
				  (GtkAttachOptions) (GTK_FILL),
				  (GtkAttachOptions) (GTK_FILL), 0, 0);
	
		description_entry = gtk_entry_new ();
		description_has_changed = FALSE;
		gtk_box_pack_start (GTK_BOX (vbox), description_entry, FALSE, FALSE, 0);
		gtk_widget_make_bold (GTK_WIDGET (description_entry));
		
		gtk_signal_connect (GTK_OBJECT (description_entry), "activate",
	      	              	    GTK_SIGNAL_FUNC (description_entry_activate),
	                            NULL);
		
		gtk_signal_connect (GTK_OBJECT (description_entry), "changed",
	      	              	    GTK_SIGNAL_FUNC (description_entry_changed),
	                            NULL);
		
		gtk_signal_connect (GTK_OBJECT (description_entry), "focus_out_event",
	      	              	    GTK_SIGNAL_FUNC (description_entry_lost_focus),
	                            NULL);
		
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (hbox), FALSE, FALSE, 0);

		mime_label = GTK_LABEL (gtk_label_new (_("MIME Type")));	
		gtk_label_set_justify (GTK_LABEL (mime_label), GTK_JUSTIFY_LEFT);
		gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (mime_label), FALSE, FALSE, 0);

		hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
		gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

		button = gtk_button_new_with_label (_("Change Icon"));
		gtk_signal_connect (GTK_OBJECT (button), "clicked", change_icon_clicked, icon_entry);
		gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

		/* spacer */
		gtk_box_pack_start (GTK_BOX (hbox), gtk_vbox_new (FALSE, 0), FALSE, FALSE, 10);

		button = gtk_button_new_with_label (_("Change File Extensions"));
		gtk_signal_connect (GTK_OBJECT (button), "clicked", change_file_extensions_clicked, NULL);
		gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);

	}

	/* Set up bottom left area. */
	{
		frame = gtk_frame_new (_("Default Action:"));
		gtk_table_attach ( GTK_TABLE (table), frame, 0, 1, 1, 2,
				   (GtkAttachOptions) (GTK_FILL),
				   (GtkAttachOptions) (0), 0, 0);
		
		vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
		gtk_container_add (GTK_CONTAINER (frame), vbox);
		gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

		hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
		gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

		viewer_button = gtk_radio_button_new_with_label (NULL, _("Use Viewer"));
		gtk_box_pack_start (GTK_BOX (hbox), viewer_button, FALSE, FALSE, 0);
		gtk_signal_connect (GTK_OBJECT (viewer_button), "toggled",
				    GTK_SIGNAL_FUNC (viewer_button_toggled), NULL);

		application_button = gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (viewer_button), 
										  _("Open With Application"));
		gtk_box_pack_start (GTK_BOX (hbox), application_button, FALSE, FALSE, 0);
		gtk_signal_connect (GTK_OBJECT (application_button), "toggled",
				    GTK_SIGNAL_FUNC (application_button_toggled), NULL);

		hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
		gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

		default_menu = gtk_option_menu_new();
		gtk_box_pack_start (GTK_BOX (hbox), default_menu, TRUE, TRUE, 0);

		button = gtk_button_new_with_label (_("Edit List"));
		gtk_misc_set_padding (GTK_MISC (GTK_BIN(button)->child), 2, 1);
		gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
		gtk_signal_connect (GTK_OBJECT (button), "clicked", edit_default_clicked, mime_list);
	}

	/* Set up top right area. */
	{
		GtkWidget *separator;
		GtkWidget *small_table;
		
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_table_attach_defaults ( GTK_TABLE (table), hbox, 1, 2, 0, 1);

		small_table =  gtk_table_new (5, 1, FALSE);
		gtk_table_set_row_spacings (GTK_TABLE (small_table), 7);
		gtk_box_pack_end (GTK_BOX (hbox), small_table, FALSE, FALSE, 0);
		
		/* Placed to space top button with top of left table */
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_table_attach (GTK_TABLE (small_table), hbox, 0, 1, 0, 1,
				  (GtkAttachOptions) (GTK_FILL),
				  (GtkAttachOptions) (0), 0, 0);
		gtk_widget_set_usize (hbox, 1, 11);
				
		button = gtk_button_new_with_label (_("Add New MIME Type..."));
		gtk_signal_connect (GTK_OBJECT (button), "clicked", add_mime_clicked, NULL);
		gtk_table_attach (GTK_TABLE (small_table), button, 0, 1, 1, 2,
				  (GtkAttachOptions) (GTK_FILL),
				  (GtkAttachOptions) (0), 0, 0);

		button = gtk_button_new_with_label (_("Delete This MIME Type"));
		gtk_signal_connect (GTK_OBJECT (button), "clicked", delete_mime_clicked, NULL);
		gtk_table_attach (GTK_TABLE (small_table), button, 0, 1, 2, 3,
				  (GtkAttachOptions) (GTK_FILL),
				  (GtkAttachOptions) (0), 0, 0);

		separator = gtk_hseparator_new ();
		gtk_table_attach (GTK_TABLE (small_table), separator, 0, 1, 3, 4,
				  (GtkAttachOptions) (GTK_FILL),
				  (GtkAttachOptions) (GTK_FILL), 0, 0);

		button = gtk_button_new_with_label (_("Revert to System Defaults"));
		gtk_signal_connect (GTK_OBJECT (button), "clicked", revert_mime_clicked, NULL);
		gtk_table_attach (GTK_TABLE (small_table), button, 0, 1, 4, 5,
				  (GtkAttachOptions) (GTK_FILL),
				  (GtkAttachOptions) (0), 0, 0);

	}

	/* Yes, show all widgets */
	gtk_widget_show_all (capplet);

	/* Make columns all fit within capplet list view bounds */
	list_width = GTK_WIDGET (mime_list)->allocation.width;
	column_width = list_width / TOTAL_COLUMNS;
	for (index = 0; index < TOTAL_COLUMNS; index++) {
		gtk_clist_set_column_width (GTK_CLIST (mime_list), index, column_width);
	}

        /* Setup capplet signals */
        gtk_signal_connect(GTK_OBJECT(capplet), "ok",
                           GTK_SIGNAL_FUNC(ok_callback), NULL);

        gtk_signal_connect(GTK_OBJECT(capplet), "help",
                           GTK_SIGNAL_FUNC(help_callback), NULL);

	gtk_signal_connect (GTK_OBJECT (mime_list),"select_row",
       	                   GTK_SIGNAL_FUNC (mime_list_selected_row_callback), NULL);

	/* Sort by description. The description is the first column in the list. */
	gtk_clist_set_sort_column (GTK_CLIST (mime_list), COLUMN_DESCRIPTION);
	gtk_clist_sort (GTK_CLIST (mime_list));
	GTK_CLIST (mime_list)->sort_type = GTK_SORT_ASCENDING;

	/* Set up initial column click tracking state. We do this so the initial clicks on
	 * columns will allow us to set the proper sort state for the user.
	 */
	sort_column_clicked[0] = TRUE; /* First sort column has been click by us in setup code */
	for (index = 1; index < TOTAL_COLUMNS; index++) {
		sort_column_clicked[index] = FALSE;
	}
	
	/* Attempt to select specified mime type in list */
	if (scroll_to_mime_type != NULL) {		
		found_index = find_row_for_mime_type (scroll_to_mime_type, GTK_CLIST (mime_list));
		if (found_index != -1) {
			gtk_clist_select_row (GTK_CLIST (mime_list), found_index, 1);
			list_reveal_row (GTK_CLIST (mime_list), found_index);
		} else {
			gtk_clist_select_row (GTK_CLIST (mime_list), 0, 1);
			list_reveal_row (GTK_CLIST (mime_list), 0);
		}			
	} else {
		gtk_clist_select_row (GTK_CLIST (mime_list), 0, 0);
		list_reveal_row (GTK_CLIST (mime_list), 0);
	}
		
	/* Inform control center that our changes are immediate */
	capplet_widget_changes_are_immediate (CAPPLET_WIDGET (capplet));
}

static gboolean
is_full_path (const char *path_or_name)
{
	return path_or_name[0] == '/';
}

static char *
capplet_get_icon_path (const char *path_or_name)
{
	char *result;
	char *alternate_relative_filename;

	if (is_full_path (path_or_name) && g_file_exists (path_or_name)) {
		return g_strdup (path_or_name);
	}

	result = gnome_vfs_icon_path_from_filename (path_or_name);
	if (result != NULL) {
		return result;
	}

	/* FIXME bugzilla.eazel.com 639:
	 * It is somewhat evil to special-case the nautilus directory here.
	 * We should clean this up if/when we come up with a way to handle
	 * Nautilus themes here.
	 */
	alternate_relative_filename = g_strconcat ("nautilus/", path_or_name, NULL);
	result = gnome_vfs_icon_path_from_filename (alternate_relative_filename);
	g_free (alternate_relative_filename);
	if (result != NULL) {
		return result;
	}

	/* FIXME bugzilla.eazel.com 639:
	 * To work correctly with Nautilus themed icons, if there's no
	 * suffix we will also try looking in the nautilus dir for a ".png" name.
	 * This will return the icon for the default theme; there is no
	 * mechanism for getting a themed icon in the capplet.
	 */
	alternate_relative_filename = g_strconcat ("nautilus/", path_or_name, ".png", NULL);
	result = gnome_vfs_icon_path_from_filename (alternate_relative_filename);
	g_free (alternate_relative_filename);

	return result;
}

/*
 *  nautilus_mime_type_capplet_update_info
 *
 *  Update controls with info based on mime type	
 */
 
void
nautilus_mime_type_capplet_update_info (const char *mime_type) {

	GnomeVFSMimeAction *action;
	const char *icon_name, *description;
	char *path;
	
	/* Update text items */
	gtk_label_set_text (GTK_LABEL (mime_label), mime_type);

	description = gnome_vfs_mime_get_description (mime_type);	
	gtk_entry_set_text (GTK_ENTRY (description_entry), description != NULL ? description : "");
	description_has_changed = FALSE;

	/* Update menus */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (application_button))) {
		populate_application_menu (default_menu, mime_type);						
	} else {
		populate_viewer_menu (default_menu, mime_type);						
	}

	/* Update extensions list */
	if (extension_list != NULL) {
		populate_extension_list (mime_type, GTK_CLIST (extension_list));
	}

	/* Set icon for mime type */
	icon_name = gnome_vfs_mime_get_icon (mime_type);
	path = NULL;
	if (icon_name != NULL) {
		path = capplet_get_icon_path (icon_name);
	}
	if (path == NULL) {
		/* No custom icon specified, or custom icon not found, use default */
		path = capplet_get_icon_path (DEFAULT_REGULAR_ICON);
	}
	nautilus_mime_type_icon_entry_set_icon (NAUTILUS_MIME_ICON_ENTRY (icon_entry), path);
	g_free (path);

	/* Indicate default action */	
	action = gnome_vfs_mime_get_default_action (mime_type);
	if (action != NULL) {
		switch (action->action_type) {
			case GNOME_VFS_MIME_ACTION_TYPE_APPLICATION:
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (application_button), TRUE);
				break;

			case GNOME_VFS_MIME_ACTION_TYPE_COMPONENT:
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (viewer_button), TRUE);
				break;
				
			default:
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (application_button), TRUE);
				break;
		}
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (application_button), TRUE);
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
	update_mime_list_action (mime_type);
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
			gnome_vfs_mime_application_free (default_app);
		}
	}
	
	gtk_option_menu_set_menu (GTK_OPTION_MENU (default_menu), new_menu);
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
	update_mime_list_action (mime_type);
}

static void
populate_viewer_menu (GtkWidget *component_menu, const char *mime_type)
{
	GtkWidget *new_menu, *menu_item;
	GList *component_list, *copy_list;
	OAF_ServerInfo *default_component;
	OAF_ServerInfo *info;
	gboolean has_none, found_match;
	char *mime_copy, *component_name;
	const char *iid;
	GList *children;
	int index;

	has_none = TRUE;
	found_match = FALSE;

	mime_copy = g_strdup (mime_type);
	
	new_menu = gtk_menu_new ();
	
	/* Get the default component */
	default_component = gnome_vfs_mime_get_default_component (mime_type);

	/* Get the component short list */
	component_list = gnome_vfs_mime_get_short_list_components (mime_type);
	if (component_list != NULL) {
		for (copy_list = component_list; copy_list != NULL; copy_list = copy_list->next) {
			has_none = FALSE;

			component_name = name_from_oaf_server_info (copy_list->data);
			menu_item = gtk_menu_item_new_with_label (component_name);
			g_free (component_name);

			/* Store copy of component name and mime type in item; free when item destroyed. */
			info = copy_list->data;
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

	/* Find all components or add a "None" item */
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
				/* Have menu appear with default component selected */
				gtk_menu_set_active (GTK_MENU (new_menu), index);
			} else {
				/* No match found.  We need to insert a menu item
				 * and add the component to the default list */
				component_name = name_from_oaf_server_info (default_component);
				menu_item = gtk_menu_item_new_with_label (component_name);
				g_free (component_name);

				/* Store copy of component name and mime type in item; free when item destroyed. */
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
			CORBA_free (default_component);
		}
	}
	
	gtk_option_menu_set_menu (GTK_OPTION_MENU (default_menu), new_menu);
}

static void
revert_real_cb (gint reply, gpointer data)
{
	if (reply == 0) {
		/* YES */
		GList *mime_types_list;
		gnome_vfs_mime_reset ();
		
		gnome_vfs_mime_info_reload ();

		mime_types_list = gnome_vfs_get_registered_mime_types ();
		
		gtk_clist_freeze (GTK_CLIST (mime_list));
		gtk_clist_clear (GTK_CLIST (mime_list));
		populate_mime_list (mime_types_list, GTK_CLIST (mime_list));
				
		/* Sort list using current sort type and select the first item. */
		gtk_clist_sort (GTK_CLIST (mime_list));
		gtk_clist_select_row (GTK_CLIST (mime_list), 0, 0);
		list_reveal_row (GTK_CLIST (mime_list), 0);
		
		gtk_clist_thaw (GTK_CLIST (mime_list));

	} else {
		/* NO */
	}

}

static void
revert_mime_clicked (GtkWidget *widget, gpointer data)
{
	GtkWidget *dialog;
	
	dialog = gnome_question_dialog_modal (_("Reverting to system settings will lose any changes\n"
						"you have ever made to File Types and Programs.\n"
						"Revert anyway?"), 
					      revert_real_cb, NULL);

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

	gnome_vfs_mime_registered_mime_type_delete (mime_type);
}

static void
add_mime_clicked (GtkWidget *widget, gpointer data)
{
	char *text[4], *tmp_text;
	const char *description;
	char *extensions, *mime_string, *filename;
        gint row;
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *bitmap;
	GnomeVFSMimeAction *action;
	GnomeVFSMimeApplication *default_app;
	OAF_ServerInfo *default_component;
	int found_index;
	
	mime_string = nautilus_mime_type_capplet_show_new_mime_window ();
	if (mime_string != NULL && mime_string[0] != '\0') {
		/* Add new type to mime list */
		pixbuf = NULL;
			
		/* Add description to first column */
		description = gnome_vfs_mime_get_description (mime_string);	
		if (description != NULL && strlen (description) > 0) {
			text[0] = g_strdup (description);
		} else {
			text[0] = g_strdup ("");
		}

		/* Add mime type to second column */
		text[1] = g_strdup (mime_string);

		/* Add extension to third columns */
		extensions = gnome_vfs_mime_get_extensions_pretty_string (mime_string);
		if (extensions != NULL) {
			text[2] = g_strdup (extensions);
		} else {
			text[2] = g_strdup ("");
		}

		/* Add default action to fourth column */
		text[3] = g_strdup(_("none"));

		/* Insert item into list */
		row = gtk_clist_insert (GTK_CLIST (mime_list), 1, text);
	        gtk_clist_set_row_data (GTK_CLIST (mime_list), row, g_strdup (mime_string));

		/* Set description column icon */
		pixbuf = capplet_get_icon_pixbuf (mime_string, FALSE);

		if (pixbuf != NULL) {
			pixbuf = capplet_gdk_pixbuf_scale_to_fit (pixbuf, MAX_ICON_WIDTH_IN_LIST, MAX_ICON_HEIGHT_IN_LIST);
			gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &bitmap, 100);
			gtk_clist_set_pixtext (GTK_CLIST (mime_list), row, 0, text[0], 5, pixmap, bitmap);
			gdk_pixbuf_unref (pixbuf);
		}

		/* Set up action column */
		pixbuf = NULL;
		action = gnome_vfs_mime_get_default_action (mime_string);
		if (action != NULL) {
			switch (action->action_type) {
				case GNOME_VFS_MIME_ACTION_TYPE_APPLICATION:
					/* Get the default application */
					default_app = gnome_vfs_mime_get_default_application (mime_string);
					g_free (text[3]);
					text[3] = g_strdup (default_app->name);

					filename = capplet_get_icon_path (DEFAULT_ACTION_ICON);
					if (filename != NULL) {
						pixbuf = gdk_pixbuf_new_from_file (filename);
						g_free (filename);
					}

					gnome_vfs_mime_application_free (default_app);
					break;

				case GNOME_VFS_MIME_ACTION_TYPE_COMPONENT:
					/* Get the default component */
					default_component = gnome_vfs_mime_get_default_component (mime_string);
					g_free (text[3]);					
					tmp_text = name_from_oaf_server_info (default_component);
					text[3] = g_strdup_printf (_("View as %s"), tmp_text);
					g_free (tmp_text);
					filename = capplet_get_icon_path ("nautilus/gnome-library.png");
					if (filename != NULL) {
						pixbuf = gdk_pixbuf_new_from_file (filename);
						g_free (filename);
					}
					CORBA_free (default_component);
					break;
					
				default:
					gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (application_button), TRUE);
					break;
			}
		}
				
		/* Set column icon */
		if (pixbuf != NULL) {
			pixbuf = capplet_gdk_pixbuf_scale_to_fit (pixbuf, MAX_ICON_WIDTH_IN_LIST, MAX_ICON_HEIGHT_IN_LIST);
			gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &bitmap, 100);
			gtk_clist_set_pixtext (GTK_CLIST (mime_list), row, 3, text[3], 5, pixmap, bitmap);
			gdk_pixbuf_unref (pixbuf);
		}
		
		/* Sort, select and scroll to new mime type */
		gtk_clist_sort (GTK_CLIST (mime_list));
		found_index = find_row_for_mime_type (mime_string, GTK_CLIST (mime_list));
		if (found_index != -1) {
			gtk_clist_select_row (GTK_CLIST (mime_list), found_index, 1);
			list_reveal_row (GTK_CLIST (mime_list), found_index);
		}			

		g_free (text[0]);
		g_free (text[1]);
		g_free (text[2]);
		g_free (text[3]);
		g_free (extensions);
		g_free (mime_string);
	}

}

static void
edit_default_clicked (GtkWidget *widget, gpointer data)
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

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (application_button))) {
		show_edit_applications_dialog (mime_type);
	} else {
		show_edit_components_dialog (mime_type);
	}
}


void
nautilus_mime_type_capplet_update_mime_list_icon_and_description (const char *mime_string)
{
	char *text;        
	const char *description;
        gint row;
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *bitmap;
	GtkCList *clist;

	clist = GTK_CLIST (mime_list);
	
	pixbuf = NULL;
	
	row = GPOINTER_TO_INT (clist->selection->data);

	gnome_vfs_mime_info_reload ();

	/* Get description text */
	description = gnome_vfs_mime_get_description (mime_string);	
	if (description != NULL && strlen (description) > 0) {
		text = g_strdup (description);
	} else {
		text = g_strdup ("");
	}

	/* Set description column icon */
	pixbuf = capplet_get_icon_pixbuf (mime_string, FALSE);

	if (pixbuf != NULL) {
		pixbuf = capplet_gdk_pixbuf_scale_to_fit (pixbuf, MAX_ICON_WIDTH_IN_LIST, MAX_ICON_HEIGHT_IN_LIST);
		gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &bitmap, 100);
		gtk_clist_set_pixtext (clist, row, 0, text, 5, pixmap, bitmap);
		gdk_pixbuf_unref (pixbuf);
	}

	g_free (text);
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
	populate_application_menu (default_menu, mime_type);
}

/*
 * nautilus_mime_type_capplet_update_component_info
 * 
 * Update state of the components menu.  This function is called
 * when the Edit Componests dialog is closed with an OK.
 */
 
void
nautilus_mime_type_capplet_update_viewer_info (const char *mime_type)
{
	populate_viewer_menu (default_menu, mime_type);
}

static void
update_mime_list_action (const char *mime_string)
{
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *bitmap;
	GnomeVFSMimeAction *action;
	GnomeVFSMimeApplication *default_app;
	OAF_ServerInfo *default_component;
	char *text, *tmp_text, *icon_path;
	int row;
	
	pixbuf = NULL;
	row = GPOINTER_TO_INT (GTK_CLIST (mime_list)->selection->data);

	text = g_strdup(_("none"));

	action = gnome_vfs_mime_get_default_action (mime_string);
	if (action != NULL) {
		switch (action->action_type) {
			/* FIXME: Big hunks of this code are copied/pasted in several
			 * places in this file. Need to use common routines. One way
			 * to find them is to search for "nautilus/gnome-library.png"
			 */
			case GNOME_VFS_MIME_ACTION_TYPE_APPLICATION:
				/* Get the default application */
				default_app = gnome_vfs_mime_get_default_application (mime_string);
				g_free (text);
				text = g_strdup (default_app->name);							
				icon_path = capplet_get_icon_path (DEFAULT_ACTION_ICON);
				if (icon_path != NULL) {
					pixbuf = gdk_pixbuf_new_from_file (icon_path);
					g_free (icon_path);
				}
				gnome_vfs_mime_application_free (default_app);
				break;

			case GNOME_VFS_MIME_ACTION_TYPE_COMPONENT:
				/* Get the default component */
				default_component = gnome_vfs_mime_get_default_component (mime_string);
				g_free (text);
				tmp_text = name_from_oaf_server_info (default_component);
				text = g_strdup_printf (_("View as %s"), tmp_text);
				g_free (tmp_text);
				icon_path = capplet_get_icon_path ("nautilus/gnome-library.png");
				if (icon_path != NULL) {
					pixbuf = gdk_pixbuf_new_from_file (icon_path);
					g_free (icon_path);
				}
				CORBA_free (default_component);
				break;
				
			default:
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (application_button), TRUE);				
				break;
		}
	}

	/* Set column icon */
	if (pixbuf != NULL) {
		pixbuf = capplet_gdk_pixbuf_scale_to_fit (pixbuf, MAX_ICON_WIDTH_IN_LIST, MAX_ICON_HEIGHT_IN_LIST);
		gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &bitmap, 100);
		gtk_clist_set_pixtext (GTK_CLIST (mime_list), row, 3, text, 5, pixmap, bitmap);
		gdk_pixbuf_unref (pixbuf);
	} else {
		/* Just set text with no icon */
		gtk_clist_set_text (GTK_CLIST (mime_list), row, 3, text);
	}
	g_free (text);
}

/* FIXME:
 * This routine is never called with is_executable TRUE anymore. It
 * could be simplified, possibly out of existence.
 */
static GdkPixbuf *
capplet_get_icon_pixbuf (const char *mime_string, gboolean is_executable)
{
	const char *icon_name;
	char *icon_path;
	GdkPixbuf *pixbuf;

	pixbuf = NULL;

	icon_name = gnome_vfs_mime_get_icon (mime_string);
	if (icon_name == NULL) {
		icon_name = is_executable
			? DEFAULT_ACTION_ICON
			: DEFAULT_REGULAR_ICON;
	}

	icon_path = capplet_get_icon_path (icon_name);
	if (icon_path != NULL) {
		pixbuf = gdk_pixbuf_new_from_file (icon_path);
		g_free (icon_path);
	}

	return pixbuf;
}

static void
populate_mime_list (GList *type_list, GtkCList *clist)
{
	char *text[4], *tmp_text;        
	const char *description;
	char *icon_path;
	char *extensions, *mime_string;
        gint row;
	GList *element;
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *bitmap;
	GnomeVFSMimeAction *action;
	GnomeVFSMimeApplication *default_app;
	OAF_ServerInfo *default_component;

	for (element = type_list; element != NULL; element= element->next) {
		mime_string = (char *)element->data;
		
		pixbuf = NULL;
			
		/* Add description to first column */
		description = gnome_vfs_mime_get_description (mime_string);	
		if (description != NULL && strlen (description) > 0) {
			text[0] = g_strdup (description);
		} else {
			text[0] = g_strdup ("");
		}

		/* Add mime type to second column */
		text[1] = mime_string;

		/* Add extension to third columns */
		extensions = gnome_vfs_mime_get_extensions_pretty_string (mime_string);
		if (extensions != NULL) {
			text[2] = extensions;
		} else {
			text[2] = "";
		}
		
		/* Add default action to fourth column */
		text[3] = g_strdup(_("none"));
		
		/* Insert item into list */
		row = gtk_clist_insert (GTK_CLIST (clist), 1, text);
		gtk_clist_set_row_data (GTK_CLIST (clist), row, g_strdup (mime_string));
		
		/* Set description column icon */
		pixbuf = capplet_get_icon_pixbuf (mime_string, FALSE);
		
		if (pixbuf != NULL) {
			/* FIXME: Big hunks of this code are copied/pasted in several
			 * places in this file. Need to use common routines. One way
			 * to find them is to search for MAX_ICON_WIDTH_IN_LIST
			 */
			pixbuf = capplet_gdk_pixbuf_scale_to_fit (pixbuf, MAX_ICON_WIDTH_IN_LIST, MAX_ICON_HEIGHT_IN_LIST);
			gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &bitmap, 100);
			gtk_clist_set_pixtext (clist, row, 0, text[0], 5, pixmap, bitmap);
			gdk_pixbuf_unref (pixbuf);
		}

		/* Set up action column */
		pixbuf = NULL;
		action = gnome_vfs_mime_get_default_action (mime_string);
		if (action != NULL) {				
			switch (action->action_type) {
			case GNOME_VFS_MIME_ACTION_TYPE_APPLICATION:
				/* Get the default application */
				default_app = gnome_vfs_mime_get_default_application (mime_string);						
				g_free (text[3]);
				text[3] = g_strdup (default_app->name);
				
				icon_path = capplet_get_icon_path (DEFAULT_ACTION_ICON);
				if (icon_path != NULL) {
					pixbuf = gdk_pixbuf_new_from_file (icon_path);
					g_free (icon_path);
				}
				gnome_vfs_mime_application_free (default_app);
				break;

			case GNOME_VFS_MIME_ACTION_TYPE_COMPONENT:
				/* Get the default component */
				default_component = gnome_vfs_mime_get_default_component (mime_string);
				g_free (text[3]);								
				tmp_text = name_from_oaf_server_info (default_component);
				text[3] = g_strdup_printf (_("View as %s"), tmp_text);
				g_free (tmp_text);
				icon_path = capplet_get_icon_path ("nautilus/gnome-library.png");
				if (icon_path != NULL) {
					pixbuf = gdk_pixbuf_new_from_file (icon_path);
					g_free (icon_path);
				}
				CORBA_free (default_component);
				break;
				
			default:
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (application_button), TRUE);
				break;
			}
		}
			
		/* Set column icon */
		if (pixbuf != NULL) {
			pixbuf = capplet_gdk_pixbuf_scale_to_fit (pixbuf, MAX_ICON_WIDTH_IN_LIST, MAX_ICON_HEIGHT_IN_LIST);
			gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, &bitmap, 100);
			gtk_clist_set_pixtext (clist, row, 3, text[3], 5, pixmap, bitmap);
			gdk_pixbuf_unref (pixbuf);
		}
		
		g_free (text[0]);
		g_free (text[3]);
		g_free (extensions);			
	}
}

static gint
sort_case_insensitive (GtkCList *clist, gpointer ptr1, gpointer ptr2)
{
	const char *text1 = NULL;
	const char *text2 = NULL;
	
	GtkCListRow *row1 = (GtkCListRow *) ptr1;
	GtkCListRow *row2 = (GtkCListRow *) ptr2;
	
	switch (row1->cell[clist->sort_column].type) {
		case GTK_CELL_TEXT:
			text1 = GTK_CELL_TEXT (row1->cell[clist->sort_column])->text;
			break;
		
		case GTK_CELL_PIXTEXT:
			text1 = GTK_CELL_PIXTEXT (row1->cell[clist->sort_column])->text;
			break;
		
		default:
			break;
	}
	
	switch (row2->cell[clist->sort_column].type) {
		case GTK_CELL_TEXT:
			text2 = GTK_CELL_TEXT (row2->cell[clist->sort_column])->text;
			break;
			
		case GTK_CELL_PIXTEXT:
			text2 = GTK_CELL_PIXTEXT (row2->cell[clist->sort_column])->text;
			break;
	
		default:
			break;
	}
	
	if (text2 == NULL) {
		return (text1 != NULL);
	}
	
	if (text1 == NULL) {
		return -1;
	}
	
	return strcasecmp (text1, text2);
}

static void
column_clicked (GtkCList *clist, gint column, gpointer user_data)
{
	gtk_clist_set_sort_column (clist, column);

	/* If the user has not clicked the column yet, make sure
	 * that the sort type is descending the first time.
	 */
	 if (!sort_column_clicked [column]) {
		clist->sort_type = GTK_SORT_DESCENDING;
		sort_column_clicked [column] = TRUE;
	}
		
	/* Toggle sort type */
	if (clist->sort_type == GTK_SORT_ASCENDING) {
		gtk_clist_set_sort_type (clist, GTK_SORT_DESCENDING);
	} else {
		gtk_clist_set_sort_type (clist, GTK_SORT_ASCENDING);
	}
	
	gtk_clist_sort (clist);
}

static void
mime_list_reset_row_height (GtkCList *list)
{
	guint height_for_icon;
	guint height_for_text;

	height_for_icon = MAX_ICON_HEIGHT_IN_LIST + 1;
	height_for_text = GTK_WIDGET (list)->style->font->ascent +
			  GTK_WIDGET (list)->style->font->descent + 1;
	gtk_clist_set_row_height (list, MAX (height_for_icon, height_for_text));
}

static GtkWidget *
create_mime_list_and_scroller (void)
{
        GtkWidget *window;
        gchar *titles[TOTAL_COLUMNS];
	GList *type_list;
	int index;
	        
        titles[0] = _("Description");
        titles[1] = _("MIME Type");
        titles[2] = _("Extension");
        titles[3] = _("Default Action");
        
        window = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (window),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
	mime_list = gtk_clist_new_with_titles (TOTAL_COLUMNS, titles);
        gtk_clist_set_selection_mode (GTK_CLIST (mime_list), GTK_SELECTION_BROWSE);
        gtk_clist_set_compare_func (GTK_CLIST (mime_list), (GtkCListCompareFunc) sort_case_insensitive);

	type_list = gnome_vfs_get_registered_mime_types ();
	populate_mime_list (type_list, GTK_CLIST (mime_list));
	gnome_vfs_mime_registered_mime_type_list_free (type_list);
	
        gtk_clist_columns_autosize (GTK_CLIST (mime_list));
        gtk_clist_select_row (GTK_CLIST (mime_list), 0, 0);
        gtk_container_add (GTK_CONTAINER (window), mime_list);

        /* Enable all titles */
	gtk_clist_column_titles_active (GTK_CLIST (mime_list));
	gtk_signal_connect (GTK_OBJECT (mime_list), "click_column", 
			    column_clicked, NULL);
	
	/* Turn off autoresizing of columns */
	for (index = 0; index < TOTAL_COLUMNS; index++) {
		gtk_clist_set_column_auto_resize (GTK_CLIST (mime_list), index, FALSE);
	}

	/* Make height tall enough for icons to look good.
	 * This must be done after the list widget is realized, due to
	 * a bug/design flaw in nautilus_clist_set_row_height. Connecting to
	 * the "realize" signal is slightly too early, so we connect to
	 * "map".
	 */
	gtk_signal_connect (GTK_OBJECT (mime_list),
			    "map",
			    mime_list_reset_row_height,
			    NULL);
		
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
	 * the list to retrieve the data */
	row = GPOINTER_TO_INT (clist->selection->data);

	mime_type = (const char *) gtk_clist_get_row_data (clist, row);

	return mime_type;
}

/**
 * gtk_label_make_bold.
 *
 * Switches the font of label to a bold equivalent.
 * @label: The label.
 **/

static void
gtk_widget_make_bold (GtkWidget *widget)
{
	GtkStyle *style;
	GdkFont *bold_font;

	g_return_if_fail (GTK_IS_WIDGET (widget));
	style = gtk_widget_get_style (widget);

	bold_font = gdk_font_get_bold (style->font);

	if (bold_font == NULL) {
		return;
	}

	gtk_widget_set_font (widget, bold_font);
	gdk_font_unref (bold_font);
}

/**
 * gtk_widget_set_font
 *
 * Sets the font for a widget's style, managing the style objects.
 * @widget: The widget.
 * @font: The font.
 **/
static void
gtk_widget_set_font (GtkWidget *widget, GdkFont *font)
{
	GtkStyle *new_style;
	
	g_return_if_fail (GTK_IS_WIDGET (widget));
	g_return_if_fail (font != NULL);
	
	new_style = gtk_style_copy (gtk_widget_get_style (widget));

	gtk_style_set_font (new_style, font);
	
	gtk_widget_set_style (widget, new_style);
	gtk_style_unref (new_style);
}

/**
 * gtk_style_set_font
 *
 * Sets the font in a style object, managing the ref. counts.
 * @style: The style to change.
 * @font: The new font.
 **/
static void
gtk_style_set_font (GtkStyle *style, GdkFont *font)
{
	g_return_if_fail (style != NULL);
	g_return_if_fail (font != NULL);
	
	gdk_font_ref (font);
	gdk_font_unref (style->font);
	style->font = font;
}

/**
 * gdk_font_get_bold
 * @plain_font: A font.
 * Returns: A bold variant of @plain_font or NULL.
 *
 * Tries to find a bold flavor of a given font. Returns NULL if none is available.
 */
static GdkFont *
gdk_font_get_bold (const GdkFont *plain_font)
{
	const char *plain_name;
	const char *scanner;
	char *bold_name;
	int count;
	GSList *p;
	GdkFont *result;
	GdkFontPrivate *private_plain;

	private_plain = (GdkFontPrivate *)plain_font;

	if (private_plain->names == NULL) {
		return NULL;
	}


	/* -foundry-family-weight-slant-sel_width-add-style-pixels-points-hor_res-ver_res-spacing-average_width-char_set_registry-char_set_encoding */

	bold_name = NULL;
	for (p = private_plain->names; p != NULL; p = p->next) {
		plain_name = (const char *)p->data;
		scanner = plain_name;

		/* skip past foundry and family to weight */
		for (count = 2; count > 0; count--) {
			scanner = strchr (scanner + 1, '-');
			if (!scanner) {
				break;
			}
		}

		if (!scanner) {
			/* try the other names in the list */
			continue;
		}
		g_assert (*scanner == '-');

		/* copy "-foundry-family-" over */
		scanner++;
		bold_name = g_strndup (plain_name, scanner - plain_name);

		/* skip weight */
		scanner = strchr (scanner, '-');
		g_assert (scanner != NULL);

		/* add "bold" and copy everything past weight over */
		bold_name = g_strconcat (bold_name, "bold", scanner, NULL);
		break;
	}
	
	if (bold_name == NULL) {
		return NULL;
	}
	
	result = gdk_font_load (bold_name);
	g_free (bold_name);

	return result;
}


/* scale the passed in pixbuf to conform to the passed-in maximum width and height */
/* utility routine to scale the passed-in pixbuf to be smaller than the maximum allowed size, if necessary */
static GdkPixbuf *
capplet_gdk_pixbuf_scale_to_fit (GdkPixbuf *pixbuf, int max_width, int max_height)
{
	double scale_factor;
	double h_scale = 1.0;
	double v_scale = 1.0;

	int width  = gdk_pixbuf_get_width(pixbuf);
	int height = gdk_pixbuf_get_height(pixbuf);
	
	if (width > max_width) {
		h_scale = max_width / (double) width;
	}
	if (height > max_height) {
		v_scale = max_height  / (double) height;
	}
	scale_factor = MIN (h_scale, v_scale);
	
	if (scale_factor < 1.0) {
		GdkPixbuf *scaled_pixbuf;
		/* the width and scale factor are always > 0, so it's OK to round by adding here */
		int scaled_width  = floor(width * scale_factor + .5);
		int scaled_height = floor(height * scale_factor + .5);
				
		scaled_pixbuf = gdk_pixbuf_scale_simple (pixbuf, scaled_width, scaled_height, GDK_INTERP_BILINEAR);	
		gdk_pixbuf_unref (pixbuf);
		pixbuf = scaled_pixbuf;
	}
	
	return pixbuf;
}
