/* -*- mode: c; style: linux -*- */

/* selection-dialog.c
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Written by Bradford Hovinen <hovinen@helixcode.com>
 * Parts written by Jamie Zawinski <jwz@jwz.org>
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
#   include "config.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>

#include <gnome.h>
#include <parser.h>

#include "selection-dialog.h"
#include "preferences.h"

enum {
	OK_CLICKED_SIGNAL,
	LAST_SIGNAL
};

static gint selection_dialog_signals[LAST_SIGNAL] = { 0 };

typedef struct _saver_entry_t 
{
	gchar *name;
	gchar *label;
} saver_entry_t;

GList *known_savers;

static void selection_dialog_init (SelectionDialog *dialog);
static void selection_dialog_class_init (SelectionDialogClass *dialog);

static void place_screensaver_list       (SelectionDialog *dialog);

static void select_program_cb            (GtkListItem *item, 
					  SelectionDialog *dialog);
static void selection_dialog_ok_cb       (GtkWidget *widget,
					  SelectionDialog *dialog);
static void selection_dialog_cancel_cb   (GtkWidget *widget,
					  SelectionDialog *dialog);

static GList *get_known_savers           (void);

guint
selection_dialog_get_type (void)
{
	static guint selection_dialog_type = 0;

	if (!selection_dialog_type) {
		GtkTypeInfo selection_dialog_info = {
			"SelectionDialog",
			sizeof (SelectionDialog),
			sizeof (SelectionDialogClass),
			(GtkClassInitFunc) selection_dialog_class_init,
			(GtkObjectInitFunc) selection_dialog_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};

		selection_dialog_type = 
			gtk_type_unique (gnome_dialog_get_type (), 
					 &selection_dialog_info);
	}

	return selection_dialog_type;
}

static void
selection_dialog_init (SelectionDialog *dialog) 
{
	GtkWidget *label, *scrolled_window, *viewport;
	GtkBox *vbox;

	gtk_window_set_title (GTK_WINDOW (dialog), _("Add a new screensaver"));

	gnome_dialog_append_button (GNOME_DIALOG (dialog),
				    GNOME_STOCK_BUTTON_OK);
	gnome_dialog_append_button (GNOME_DIALOG (dialog),
				    GNOME_STOCK_BUTTON_CANCEL);

	vbox = GTK_BOX (GNOME_DIALOG (dialog)->vbox);

	label = gtk_label_new (_("Select the screensaver to run from " \
				 "the list below:"));
	gtk_box_pack_start (vbox, label, FALSE, TRUE, 0);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_box_pack_start (vbox, scrolled_window, TRUE, TRUE, 0);

	viewport = gtk_viewport_new (NULL, NULL);
	gtk_container_add (GTK_CONTAINER (scrolled_window), viewport);

	dialog->program_list = GTK_LIST (gtk_list_new ());
	gtk_list_set_selection_mode (GTK_LIST (dialog->program_list), 
				     GTK_SELECTION_SINGLE);
	gtk_container_add (GTK_CONTAINER (viewport), 
			   GTK_WIDGET (dialog->program_list));

	gnome_dialog_button_connect (GNOME_DIALOG (dialog), 0,
				     GTK_SIGNAL_FUNC (selection_dialog_ok_cb),
				     dialog);
	gnome_dialog_button_connect (GNOME_DIALOG (dialog), 1,
				     GTK_SIGNAL_FUNC 
				     (selection_dialog_cancel_cb),
				     dialog);

	gtk_widget_show_all (GNOME_DIALOG (dialog)->vbox);
}

static void
selection_dialog_class_init (SelectionDialogClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;
    
	selection_dialog_signals[OK_CLICKED_SIGNAL] =
		gtk_signal_new ("ok-clicked", GTK_RUN_FIRST, 
				object_class->type,
				GTK_SIGNAL_OFFSET (SelectionDialogClass, 
						   ok_clicked),
				gtk_marshal_NONE__POINTER, 
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, 
				      selection_dialog_signals,
				      LAST_SIGNAL);

	known_savers = get_known_savers ();

	class->ok_clicked = NULL;
}

GtkWidget *
selection_dialog_new (PrefsWidget *prefs_widget) 
{
	GtkWidget *widget;

	widget = gtk_type_new (selection_dialog_get_type ());
	place_screensaver_list (SELECTION_DIALOG (widget));
	gtk_widget_show (widget);

	return widget;
}

static void
place_screensaver_list (SelectionDialog *dialog) 
{
	GtkWidget *item;
	GList *item_list_head = NULL, *item_list_tail = NULL, *node;
	saver_entry_t *entry;

	for (node = known_savers; node; node = node->next) {
		entry = (saver_entry_t *) node->data;
		item = gtk_list_item_new_with_label (entry->label);
		gtk_widget_show (item);
		gtk_object_set_data (GTK_OBJECT (item), "name", entry->name);
		gtk_signal_connect (GTK_OBJECT (item), "select",
				    GTK_SIGNAL_FUNC (select_program_cb), 
				    dialog);

		item_list_tail = g_list_append (item_list_tail, item);
		if (!item_list_head) item_list_head = item_list_tail;
		item_list_tail = g_list_last (item_list_tail);
	}

	item = gtk_list_item_new_with_label (_("Custom"));
	gtk_widget_show (item);
	gtk_signal_connect (GTK_OBJECT (item), "select",
			    GTK_SIGNAL_FUNC (select_program_cb), dialog);

	item_list_tail = g_list_append (item_list_tail, item);
	if (!item_list_head) item_list_head = item_list_tail;
	gtk_list_append_items (dialog->program_list, item_list_head);

	gtk_list_select_item (dialog->program_list, 0);
}

static void 
select_program_cb (GtkListItem *item, SelectionDialog *dialog) 
{
	dialog->selected_program_item = item;
	dialog->selected_name =
		gtk_object_get_data (GTK_OBJECT (item), "name");
}

static void 
selection_dialog_ok_cb (GtkWidget *widget, SelectionDialog *dialog) 
{
	Screensaver *saver;

	saver = screensaver_new ();
	saver->label = g_strdup (_("New screensaver"));

	if (dialog->selected_name) {
		saver->name = 
			g_strdup (dialog->selected_name);
		saver->command_line = 
			g_strconcat (saver->name, " -root", NULL);
	}

	gtk_signal_emit (GTK_OBJECT (dialog),
			 selection_dialog_signals[OK_CLICKED_SIGNAL], saver);
	gnome_dialog_close (GNOME_DIALOG (dialog));
}

static void 
selection_dialog_cancel_cb (GtkWidget *widget, SelectionDialog *dialog) 
{
	gnome_dialog_close (GNOME_DIALOG (dialog));
}

static gint 
node_compare (gconstpointer a, gconstpointer b) 
{
	return strcmp (((saver_entry_t *) a)->label, 
		       ((saver_entry_t *) b)->label);
}

static GList *
get_known_savers (void)
{
	GList *list_head, *list_tail;
        DIR *parent_dir;
        struct dirent *child_dir;
        struct stat filedata;
	gchar *fullpath;
	saver_entry_t *entry = NULL;
	char *tmp, *name;

	if (known_savers) return known_savers;

        parent_dir = opendir (GNOMECC_SCREENSAVERS_DIR "/screensavers");
        if (parent_dir == NULL)
                return NULL;

	list_head = list_tail = NULL;

        while ((child_dir = readdir (parent_dir)) != NULL) {
                if (child_dir->d_name[0] != '.') {
			fullpath = g_concat_dir_and_file
				(GNOMECC_SCREENSAVERS_DIR "/screensavers",
				 child_dir->d_name);

                        if (stat (fullpath, &filedata) != -1) {
                                if (!S_ISDIR (filedata.st_mode)) {
					name = g_strdup (child_dir->d_name);
					tmp = strstr (name, ".xml");
					if (tmp) {
						*tmp = '\0';

						entry = g_new0 
							(saver_entry_t, 1);
						entry->name = name;
						entry->label =
							screensaver_get_label 
							(name);
					} else {
						g_free (name);
					}
                                } else {
					entry = NULL;
				}

				if (entry) {
					list_tail = g_list_append
						(list_tail, entry);
					if (!list_head)
						list_head = list_tail;
					else
						list_tail = list_tail->next;
				}
                        }

			g_free (fullpath);
                }
        }
        
        closedir (parent_dir);

	list_head = g_list_sort (list_head, node_compare);

	return list_head;
}
