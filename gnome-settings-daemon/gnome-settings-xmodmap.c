/* gnome-settings-xmodmap.c
 *
 * Copyright Â© 2005 Novell Inc.
 *
 * Written by Shakti Sen <shprasad@novell.com>
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



#include <glade/glade.h>
#include <gnome.h>
#include <gconf/gconf-client.h>
#include "gnome-settings-xmodmap.h"
#include "config.h"

static const char DISABLE_XMM_WARNING_KEY[] =
    "/desktop/gnome/peripherals/keyboard/disable_xmm_and_xkb_warning";

static const char LOADED_FILES_KEY[] =
    "/desktop/gnome/peripherals/keyboard/general/update_handlers";

 
static void
check_button_callback (GtkWidget *chk_button,
		       gpointer data)
{
	GConfClient *confClient = gconf_client_get_default ();
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (chk_button))) {
		gconf_client_set_bool (confClient, DISABLE_XMM_WARNING_KEY, TRUE,
				       NULL);
	}
	else {
		gconf_client_set_bool (confClient, DISABLE_XMM_WARNING_KEY, FALSE,
				       NULL);
	}

}

void
gnome_settings_load_modmap_files ()
{
	GConfClient *confClient = gconf_client_get_default ();
	GSList *tmp = NULL;
	GSList *loaded_file_list = gconf_client_get_list (confClient, LOADED_FILES_KEY, GCONF_VALUE_STRING, NULL);
	tmp = loaded_file_list;
	while (tmp != NULL) {
		gchar *command = NULL;
		command = g_strdup_printf ("xmodmap %s", g_build_filename (g_get_home_dir (), (gchar *)tmp->data, NULL));
		g_spawn_command_line_async (command, NULL);
		tmp = tmp->next;
		g_free (command);
	}
}

static void
response_callback (GtkWidget *dialog,
		   int        id,
		   void      *data)
{
	if (id == GTK_RESPONSE_OK) {
		GtkWidget *chk_button = g_object_get_data (G_OBJECT (dialog), "check_button");
		check_button_callback (chk_button, NULL);
		gnome_settings_load_modmap_files ();
	}
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
get_selected_files_func (GtkTreeModel      *model,
                             GtkTreePath       *path,
                             GtkTreeIter       *iter,
                             gpointer           data)
{
	GSList **list = data;
	gchar *filename;

	filename = NULL;
	gtk_tree_model_get (model,
			    iter,
			    0,
			    &filename,
			    -1);

	*list = g_slist_prepend (*list, filename);
}

static GSList*
remove_string_from_list (GSList     *list,
                         const char *str)
{
	GSList *tmp;

	tmp = list;
	while (tmp != NULL) {
		if (strcmp (tmp->data, str) == 0)
			break;

		tmp = tmp->next;
	}

	if (tmp != NULL) {
		g_free (tmp->data);
		list = g_slist_remove (list, tmp->data);
	}

	return list;
}


static void
remove_button_clicked_callback (GtkWidget *button,
                             void      *data)
{
	GladeXML *xml;
	GtkWidget *dialog;
	GtkListStore *tree = NULL;
	GtkTreeSelection *selection;
	GtkWidget *treeview;
	GConfClient *confClient;
	GSList *filenames = NULL;
	GSList *tmp = NULL;
	GSList *loaded_files = NULL;

	dialog = data;

	xml = g_object_get_data (G_OBJECT (dialog), "treeview1");
	treeview = glade_xml_get_widget (xml, "treeview1");

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	gtk_tree_selection_selected_foreach (selection,
						get_selected_files_func,
						&filenames);

	if (!filenames)
                return;

	/* Remove the selected file */
	confClient = gconf_client_get_default ();
	loaded_files = gconf_client_get_list (confClient, LOADED_FILES_KEY, GCONF_VALUE_STRING, NULL);
	loaded_files = remove_string_from_list (loaded_files, (char *)filenames->data);

	gconf_client_set_list (confClient, LOADED_FILES_KEY, GCONF_VALUE_STRING, loaded_files,
			       NULL);
	tree = g_object_get_data (G_OBJECT (dialog), "tree");

	gtk_list_store_clear (tree);
	tmp = loaded_files;
	while (tmp != NULL) {
		GtkTreeIter iter;
		gtk_list_store_append (tree, &iter);
		gtk_list_store_set (tree, &iter,
				    0,
				    (char *)tmp->data,
				    -1);
		tmp = tmp->next;
	}

	g_slist_foreach (loaded_files, (GFunc) g_free, NULL);
        g_slist_free (loaded_files);

        g_object_unref (G_OBJECT (confClient));
}

static void
load_button_clicked_callback (GtkWidget *button,
                             void      *data)
{
	GtkWidget *dialog;
	GtkListStore *tree = NULL;
	GtkTreeSelection *selection;
	GtkWidget *treeview;
	GSList *filenames = NULL;
	GSList *tmp = NULL;
	GSList *loaded_files = NULL;
	GConfClient *confClient; 

	dialog = data;

	treeview = g_object_get_data (G_OBJECT (dialog),
				      "loaded-treeview");
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	gtk_tree_selection_selected_foreach (selection,
						get_selected_files_func,
						&filenames);

	if (!filenames)
                return;

	/* Add the files to left-tree-view */

	confClient = gconf_client_get_default ();
	loaded_files = gconf_client_get_list (confClient, LOADED_FILES_KEY, GCONF_VALUE_STRING, NULL);
	tmp = loaded_files;
	while (tmp != NULL) {
		if (strcmp (tmp->data, (char *)filenames->data) == 0)
			return;;

		tmp = tmp->next;

	}

	loaded_files = g_slist_append (loaded_files, (char *)filenames->data);
	gconf_client_set_list (confClient, LOADED_FILES_KEY,
			       GCONF_VALUE_STRING, loaded_files,
			       NULL);
	tree = g_object_get_data (G_OBJECT (dialog), "tree");

	gtk_list_store_clear (tree);
	tmp = loaded_files;
	while (tmp != NULL) {
		GtkTreeIter iter;
		gtk_list_store_append (tree, &iter);
		gtk_list_store_set (tree, &iter,
				    0,
				    (char *)tmp->data,
				    -1);
		tmp = tmp->next;
 	}
	g_slist_foreach (loaded_files, (GFunc) g_free, NULL);
        g_slist_free (loaded_files);

        g_object_unref (G_OBJECT (confClient));
}
 
void
gnome_settings_modmap_dialog_call (void)
{
	GladeXML *xml;
	GtkWidget *load_dialog;
	GtkListStore *tree;
	GtkCellRenderer *cell_renderer;
	GtkTreeIter parent_iter;
	GtkTreeIter iter;
	GtkTreeModel *sort_model;
	GtkTreeSelection *selection;
	GtkWidget *treeview;
	GtkWidget *treeview1;
	GtkTreeViewColumn *column;
	GtkWidget *add_button;
	GtkWidget *remove_button;
	GtkWidget *chk_button;
	GSList *tmp = NULL;
	GDir *homeDir;
	GSList *loaded_files = NULL;
	G_CONST_RETURN gchar *fname;
	GConfClient *confClient = gconf_client_get_default ();
	homeDir = g_dir_open (g_get_home_dir (), 0, NULL);
	if (homeDir == NULL)
		return;
	xml = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/modmap-dialog.glade", "dialog1", NULL);

	if (! xml) {
		g_warning ("Could not find  GLADE_FILE \n");
		return;
	}

	load_dialog = glade_xml_get_widget (xml, "dialog1");
	gtk_window_set_modal (GTK_WINDOW (load_dialog), TRUE);
	g_signal_connect (G_OBJECT (load_dialog), "response",
			  G_CALLBACK (response_callback),
			  xml);
	add_button = glade_xml_get_widget (xml, "button7");
	g_signal_connect (G_OBJECT (add_button), "clicked",
			  G_CALLBACK (load_button_clicked_callback),
			  load_dialog);
	remove_button = glade_xml_get_widget (xml, "button6");
	g_signal_connect (G_OBJECT (remove_button), "clicked",
			  G_CALLBACK (remove_button_clicked_callback),
			  load_dialog);
	chk_button = glade_xml_get_widget (xml, "checkbutton1");
	g_signal_connect (G_OBJECT (chk_button), "toggled",
			  G_CALLBACK (check_button_callback),
			  NULL);
	g_object_set_data (G_OBJECT (load_dialog), "check_button", chk_button);
	g_object_set_data (G_OBJECT (load_dialog), "treeview1", xml);
	treeview = glade_xml_get_widget (xml, "treeview2");
	g_object_set_data (G_OBJECT (load_dialog),
			   "loaded-treeview",
			   treeview);
	tree = gtk_list_store_new (1, G_TYPE_STRING);
	cell_renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("modmap",
							   cell_renderer,
							   "text", 0,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);
	gtk_tree_view_column_set_sort_column_id (column, 0);

	/* Add the data */
	while ((fname = g_dir_read_name (homeDir)) != NULL) {
		if (g_strrstr (fname, "modmap")) {
			gtk_list_store_append (tree, &parent_iter);
			gtk_list_store_set (tree, &parent_iter,
					    0,
				            g_strdup (fname),
				            -1);
		}
	}
	sort_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (tree));
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
					      0,
					      GTK_SORT_ASCENDING);
	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), sort_model);
	g_object_unref (G_OBJECT (tree));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	gtk_tree_selection_set_mode (GTK_TREE_SELECTION (selection),
			             GTK_SELECTION_MULTIPLE);
	gtk_widget_show (load_dialog);

	g_dir_close (homeDir);

	/* Left treeview */
	treeview1 = glade_xml_get_widget (xml, "treeview1");
	tree = gtk_list_store_new (1, G_TYPE_STRING);
	cell_renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("modmap",
							   cell_renderer,
							   "text", 0,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (treeview1), column);
	gtk_tree_view_column_set_sort_column_id (column, 0);

	loaded_files = gconf_client_get_list (confClient, LOADED_FILES_KEY, GCONF_VALUE_STRING, NULL);

	/* Add the data */
	tmp = loaded_files;
	while (tmp != NULL) {
		gchar *command = NULL;
		gtk_list_store_append (tree, &iter);
		gtk_list_store_set (tree, &iter,
				    0,
				    (char *)tmp->data,
				    -1);
		tmp = tmp->next;
		g_free (command);
	}
	
	sort_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (tree));
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
					      0,
					      GTK_SORT_ASCENDING);
	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview1), sort_model);
	g_object_unref (G_OBJECT (tree));
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview1));
	gtk_tree_selection_set_mode (GTK_TREE_SELECTION (selection),
			             GTK_SELECTION_MULTIPLE);
	g_object_set_data (G_OBJECT (load_dialog), "tree", tree);

}
