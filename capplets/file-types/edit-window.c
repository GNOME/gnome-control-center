/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 *
 *  Copyright (C) 1998, 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 2000 Eazel, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Jonathan Blandford <jbr@redhat.com>
 *  	     Gene Z. Ragan <gzr@eazel.com>
 *
 */

/* edit-window.c: Mime capplet editor window */

/*#include <libgnomevfs/gnome-vfs-mime-handlers.h>*/

#include "edit-window.h"
#include "capplet-widget.h"

extern GtkWidget *capplet;

typedef struct {
	GtkWidget *window;
	GtkWidget *icon_entry;
	GtkWidget *mime_type;
	GtkWidget *regexp1_tag_label;
	GtkWidget *regexp2_tag_label;
	GtkWidget *regexp1_label;
	GtkWidget *regexp2_label;
        GtkWidget *ext_scroll;
        GtkWidget *ext_clist;
        GtkWidget *ext_entry;
        GtkWidget *ext_add_button;
        GtkWidget *ext_remove_button;
        GtkWidget *application_menu;
        GtkWidget *component_menu;
	char 	  *mime_string;
        GList 	  *tmp_ext[2];
} edit_window;

static edit_window *main_win = NULL;
static gboolean changing = TRUE;

/* Local prototypes */
static void populate_application_menu 	(GtkWidget *application_menu, const char *mime_string);
static void populate_component_menu 	(GtkWidget *application_menu, const char *mime_string);


static void
destruction_handler (GtkWidget *widget, gpointer data)
{
	g_free (main_win);
	main_win = NULL;
}

static void
entry_changed (GtkWidget *widget, gpointer data)
{
	if (changing == FALSE)
		capplet_widget_state_changed (CAPPLET_WIDGET (capplet),
					      TRUE);
}
static void
ext_clist_selected (GtkWidget *clist, gint row, gint column, gpointer data)
{
        gboolean deletable;

	deletable = GPOINTER_TO_INT (gtk_clist_get_row_data (GTK_CLIST (clist), row));
	if (deletable)
	        gtk_widget_set_sensitive (main_win->ext_remove_button, TRUE);
	else
	        gtk_widget_set_sensitive (main_win->ext_remove_button, FALSE);
}
static void
ext_clist_deselected (GtkWidget *clist, gint row, gint column, gpointer data)
{
        if (g_list_length (GTK_CLIST (clist)->selection) == 0)
	        gtk_widget_set_sensitive (main_win->ext_remove_button, FALSE);
}
static void
ext_entry_changed (GtkWidget *entry, gpointer data)
{
        gchar *text;
	text = gtk_entry_get_text (GTK_ENTRY (entry));
	gtk_widget_set_sensitive (main_win->ext_add_button, (strlen (text) >0));
}
static void
ext_add (GtkWidget *widget, gpointer data)
{
        gchar *row[1];
	gint rownumber;

	row[0] = g_strdup (gtk_entry_get_text (GTK_ENTRY (main_win->ext_entry)));
	rownumber = gtk_clist_append (GTK_CLIST (main_win->ext_clist), row);
	gtk_clist_set_row_data (GTK_CLIST (main_win->ext_clist), rownumber,
				GINT_TO_POINTER (TRUE));
	gtk_entry_set_text (GTK_ENTRY (main_win->ext_entry), "");

	main_win->tmp_ext[0] = g_list_prepend (main_win->tmp_ext[0], row[0]);
	if (changing == FALSE)
	        capplet_widget_state_changed (CAPPLET_WIDGET (capplet),
					      TRUE);
}
static void
ext_remove (GtkWidget *widget, gpointer data)
{
        gint row;
	gchar *text;
	gchar *store;
	GList *tmp;

	text = (gchar *)g_malloc (sizeof (gchar) * 1024);
	gtk_clist_freeze (GTK_CLIST (main_win->ext_clist));
	row = GPOINTER_TO_INT (GTK_CLIST (main_win->ext_clist)->selection->data);
	gtk_clist_get_text (GTK_CLIST (main_win->ext_clist), row, 0, &text);
	store = g_strdup (text);
	gtk_clist_remove (GTK_CLIST (main_win->ext_clist), row);

	gtk_clist_thaw (GTK_CLIST (main_win->ext_clist));

	for (tmp = main_win->tmp_ext[0]; tmp; tmp = tmp->next) {
	        GList *found;

		if (strcmp (tmp->data, store) == 0) {
		        found = tmp;

			main_win->tmp_ext[0] = g_list_remove_link (main_win->tmp_ext[0], found);
			g_list_free_1 (found);
			break;
		}
	}

	if (changing == FALSE)
		capplet_widget_state_changed (CAPPLET_WIDGET (capplet),
					      TRUE);
}

static void
apply_changes (const char *mime_type)
{
	if (changing == FALSE)
		capplet_widget_state_changed (CAPPLET_WIDGET (capplet),
					      TRUE);
}

static void
initialize_main_win ()
{
	GtkWidget *align, *vbox, *hbox, *vbox2, *vbox3;
	GtkWidget *frame, *table, *label;

	gchar *title[2] = {"Extensions"};

	main_win = g_new (edit_window, 1);
	main_win->window = gnome_dialog_new ("",
					     GNOME_STOCK_BUTTON_OK,
					     GNOME_STOCK_BUTTON_CANCEL,
					     NULL);

	gtk_signal_connect (GTK_OBJECT (main_win->window),
			    "destroy",
			    destruction_handler,
			    NULL);
	vbox = GNOME_DIALOG (main_win->window)->vbox;
	
	/* icon box */
	main_win->icon_entry = gnome_icon_entry_new ("mime_icon_entry", _("Select an icon..."));
	align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	gtk_container_add (GTK_CONTAINER (align), main_win->icon_entry);
	gtk_signal_connect (GTK_OBJECT (gnome_icon_entry_gtk_entry (GNOME_ICON_ENTRY (main_win->icon_entry))),
			    "changed",
			    entry_changed,
			    NULL);

			    
	gtk_box_pack_start (GTK_BOX (vbox), align, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new (_("Mime Type: ")), FALSE, FALSE, 0);
	main_win->mime_type = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hbox), main_win->mime_type, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	/* extension/regexp */
	vbox2 = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (vbox), vbox2, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	main_win->ext_clist = gtk_clist_new_with_titles (1, title);
	gtk_clist_column_titles_passive (GTK_CLIST (main_win->ext_clist));
	gtk_clist_set_auto_sort (GTK_CLIST (main_win->ext_clist), TRUE);

	gtk_signal_connect (GTK_OBJECT (main_win->ext_clist), 
			    "select-row",
			    GTK_SIGNAL_FUNC (ext_clist_selected), 
			    NULL);
	gtk_signal_connect (GTK_OBJECT (main_win->ext_clist),
			    "unselect-row",
			    GTK_SIGNAL_FUNC (ext_clist_deselected),
			    NULL);
	main_win->ext_scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (main_win->ext_scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (main_win->ext_scroll), 
			   main_win->ext_clist);

	vbox3 = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	main_win->ext_add_button = gtk_button_new_with_label (_("Add"));
	gtk_signal_connect (GTK_OBJECT (main_win->ext_add_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (ext_add),
			    NULL);
	gtk_box_pack_start (GTK_BOX (vbox3), main_win->ext_add_button, FALSE, FALSE, 0);
	gtk_widget_set_sensitive (main_win->ext_add_button, FALSE);

	main_win->ext_remove_button = gtk_button_new_with_label (_("Remove"));
	gtk_signal_connect (GTK_OBJECT (main_win->ext_remove_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (ext_remove),
			    NULL);
	gtk_widget_set_sensitive (main_win->ext_remove_button, FALSE);
	gtk_box_pack_start (GTK_BOX (vbox3), main_win->ext_remove_button,
			    FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (hbox), main_win->ext_scroll, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), vbox3, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (vbox2), hbox, TRUE, TRUE, 0);

	main_win->ext_entry = gtk_entry_new ();
	gtk_signal_connect (GTK_OBJECT (main_win->ext_entry),
			    "changed",
			    ext_entry_changed,
			    NULL);
	gtk_signal_connect (GTK_OBJECT (main_win->ext_entry),
			    "activate",
			    ext_add,
			    NULL);
	gtk_box_pack_start (GTK_BOX (vbox2), main_win->ext_entry, TRUE, TRUE, 0);

	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	main_win->regexp1_label = gtk_label_new ("");
	main_win->regexp1_tag_label = gtk_label_new (_("First Regular Expression: "));
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), main_win->regexp1_tag_label,
			    FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), main_win->regexp1_label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, GNOME_PAD_SMALL);
	main_win->regexp2_label = gtk_label_new ("");
	main_win->regexp2_tag_label = gtk_label_new (_("Second Regular Expression: "));
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), main_win->regexp2_tag_label,
			    FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), main_win->regexp2_label, FALSE, FALSE, 0);

	/* Defaults box */
	frame = gtk_frame_new (_("Launch Options"));
	vbox2 = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);
	gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);
	table = gtk_table_new (3, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table), GNOME_PAD_SMALL);
	gtk_container_set_border_width (GTK_CONTAINER (table), GNOME_PAD_SMALL);
	gtk_container_add (GTK_CONTAINER (frame), vbox2);

	/* Default application label and menu */	
	label = gtk_label_new (_("Default Application:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), 2, 0);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);

	main_win->application_menu = gtk_option_menu_new();
	gtk_misc_set_alignment (GTK_MISC (main_win->application_menu), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (main_win->application_menu), 2, 0);
	gtk_box_pack_start (GTK_BOX (vbox2), table, FALSE, FALSE, 0);
	gtk_table_attach_defaults (GTK_TABLE (table), main_win->application_menu, 1, 2, 0, 1);
	
	/* Default component label and menu */	
	label = gtk_label_new (_("Default Component:"));
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label), 2, 0);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);

	main_win->component_menu = gtk_option_menu_new();
	gtk_misc_set_alignment (GTK_MISC (main_win->component_menu), 0.0, 0.5);
	gtk_misc_set_padding (GTK_MISC (main_win->component_menu), 2, 0);
	gtk_box_pack_start (GTK_BOX (vbox2), table, FALSE, FALSE, 0);
	gtk_table_attach_defaults (GTK_TABLE (table), main_win->component_menu, 1, 2, 1, 2);	
}

void
initialize_main_win_vals (void)
{
	char *mi;
	gchar *title;
	/*gboolean showext = FALSE;*/
	if (main_win == NULL)
		return;

	mi = main_win->mime_string;
	if (mi == NULL)
		return;
		
	/* now we fill in the fields with the mi stuff. */
	changing = TRUE;
	
	populate_application_menu (main_win->application_menu, mi);
	populate_component_menu (main_win->component_menu, mi);
	
	gtk_label_set_text (GTK_LABEL (main_win->mime_type), mi);

	/*
	FIXME
	gnome_icon_entry_set_icon (GNOME_ICON_ENTRY (main_win->icon_entry),
				   gnome_mime_get_value (mi->mime_type,
							 "icon-filename"));
	*/
	
	gtk_widget_show_all (GNOME_DIALOG (main_win->window)->vbox);
	/* we initialize everything */
	title = g_strdup_printf (_("Set actions for %s"), mi);
	gtk_window_set_title (GTK_WINDOW (main_win->window), title);
	g_free (title);

	/* not sure why this is necessary */
	gtk_clist_clear (GTK_CLIST (main_win->ext_clist));
	/*
	if (mi->ext[0]) {
	        GList *tmp;
                gchar *extension[1];
		gint row;
		for (tmp = mi->ext[0]; tmp; tmp = tmp->next) {
		       extension[0] = g_strdup (tmp->data);
		       row = gtk_clist_append (GTK_CLIST (main_win->ext_clist),
					       extension);
		       gtk_clist_set_row_data (GTK_CLIST (main_win->ext_clist),
					       row, GINT_TO_POINTER (FALSE));
		}
		showext = TRUE;
	}
	if (mi->ext[1]) {
	        GList *tmp;
	        gchar *extension[1];
		gint row;
		for (tmp = mi->ext[1]; tmp; tmp = tmp->next) {
		       extension[0] = g_strdup (tmp->data);
		       row = gtk_clist_append (GTK_CLIST (main_win->ext_clist),
					       extension);
		       gtk_clist_set_row_data (GTK_CLIST (main_win->ext_clist),
					       row, GINT_TO_POINTER (FALSE));
		}
		showext = TRUE;
	}
	
	if (main_win->tmp_ext[0]) {
	        GList *tmp;
		gchar *extension[1];
		gint row;
		for (tmp = main_win->tmp_ext[0]; tmp; tmp = tmp->next) {
		       extension[0] = g_strdup (tmp->data);
		       row = gtk_clist_append (GTK_CLIST (main_win->ext_clist),
					       extension);
		       gtk_clist_set_row_data (GTK_CLIST (main_win->ext_clist),
					       row, GINT_TO_POINTER (TRUE));
		}
		showext = TRUE;
	}
	if (main_win->tmp_ext[1]) {
	        GList *tmp;
		gchar *extension[1];
		gint row;
		for (tmp = main_win->tmp_ext[0]; tmp; tmp = tmp->next) {
		       extension[0] = g_strdup (tmp->data);
		       row = gtk_clist_append (GTK_CLIST (main_win->ext_clist),
					       extension);
		       gtk_clist_set_row_data (GTK_CLIST (main_win->ext_clist),
					       row, GINT_TO_POINTER (TRUE));
		}
		showext = TRUE;
	}
	
	if (!showext) {
	        gtk_widget_hide (main_win->ext_clist);
	        gtk_widget_hide (main_win->ext_entry);
		gtk_widget_hide (main_win->ext_add_button);
		gtk_widget_hide (main_win->ext_remove_button);
		gtk_widget_hide (main_win->ext_scroll);
	}
	if (mi->regex_readable[0])
		gtk_label_set_text (GTK_LABEL (main_win->regexp1_label),
				    mi->regex_readable[0]);
	else {
		gtk_widget_hide (main_win->regexp1_label);
		gtk_widget_hide (main_win->regexp1_tag_label);
	}
	if (mi->regex_readable[1])
		gtk_label_set_text (GTK_LABEL (main_win->regexp2_label),
				    mi->regex_readable[1]);
	else {
		gtk_widget_hide (main_win->regexp2_label);
		gtk_widget_hide (main_win->regexp2_tag_label);
	}
	*/
	
	changing = FALSE;
}

void
launch_edit_window (const char *mime_type)
{
	if (main_win == NULL) {
		initialize_main_win ();
	}

	strcpy (main_win->mime_string, mime_type);
	main_win->tmp_ext[0] = NULL;
	main_win->tmp_ext[1] = NULL;
	
	initialize_main_win_vals ();

	switch(gnome_dialog_run (GNOME_DIALOG (main_win->window))) {
		case 0:
			apply_changes (mime_type);
			/* Fall through */
		case 1:
			main_win->mime_type = NULL;
			gtk_widget_hide (main_win->window);
			break;
	}
}

void
hide_edit_window (void)
{
	if (main_win && main_win->mime_string && main_win->window)
		gtk_widget_hide (main_win->window);
}
void
show_edit_window (void)
{
	if (main_win && main_win->mime_string && main_win->window)
		gtk_widget_show (main_win->window);
}





static void
populate_application_menu (GtkWidget *application_menu, const char *mime_string)
{
	GtkWidget *new_menu;
	GtkWidget *menu_item;

	/*
	GList *mime_list;
	mime_list = gnome_vfs_mime_get_short_list_applications (mime_string);
	*/
	
	new_menu = gtk_menu_new ();

	menu_item = gtk_menu_item_new_with_label ("Test Menu One");
	gtk_menu_append (GTK_MENU (new_menu), menu_item);
	gtk_widget_show (menu_item);

	menu_item = gtk_menu_item_new_with_label ("Test Menu Two");
	gtk_menu_append (GTK_MENU (new_menu), menu_item);
	gtk_widget_show (menu_item);

	menu_item = gtk_menu_item_new_with_label ("This is a reallt long test menu item.");
	gtk_menu_append (GTK_MENU (new_menu), menu_item);
	gtk_widget_show (menu_item);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (application_menu), new_menu);
}


static void
populate_component_menu (GtkWidget *component_menu, const char *mime_string)
{
	GtkWidget *new_menu;
	GtkWidget *menu_item;
		
	new_menu = gtk_menu_new ();

	menu_item = gtk_menu_item_new_with_label ("Test Menu One");
	gtk_menu_append (GTK_MENU (new_menu), menu_item);
	gtk_widget_show (menu_item);

	menu_item = gtk_menu_item_new_with_label ("Test Menu Two");
	gtk_menu_append (GTK_MENU (new_menu), menu_item);
	gtk_widget_show (menu_item);

	menu_item = gtk_menu_item_new_with_label ("This is a reallt long test menu item.");
	gtk_menu_append (GTK_MENU (new_menu), menu_item);
	gtk_widget_show (menu_item);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (component_menu), new_menu);
}
