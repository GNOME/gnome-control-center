/* -*- mode: c; style: linux -*- */
/* -*- c-basic-offset: 2 -*- */

/* sound-theme.c
 * Copyright (C) 2008 Bastien Nocera <hadess@hadess.net>
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

#include <config.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <glade/glade.h>
#include <gconf/gconf-client.h>
#include <canberra-gtk.h>

#include "sound-theme.h"
#include "capplet-util.h"
#include "sound-theme-definition.h"
#include "sound-theme-file-utils.h"

#define SOUND_THEME_KEY		"/desktop/gnome/sound/theme_name"
#define INPUT_SOUNDS_KEY	"/desktop/gnome/sound/input_feedback_sounds"
#define VISUAL_BELL_KEY		"/apps/metacity/general/visual_bell"
#define VISUAL_BELL_TYPE_KEY	"/apps/metacity/general/visual_bell_type"
#define AUDIO_BELL_KEY		"/apps/metacity/general/audible_bell"

#define CUSTOM_THEME_NAME	"__custom"
#define PREVIEW_BUTTON_XPAD	5

enum {
	THEME_DISPLAY_COL,
	THEME_IDENTIFIER_COL,
	THEME_PARENT_ID_COL,
	THEME_NUM_COLS
};

enum {
	SOUND_UNSET,
	SOUND_OFF,
	SOUND_BUILTIN,
	SOUND_CUSTOM,
	SOUND_CUSTOM_OLD
};

#define SOUND_VISUAL_BELL_TITLEBAR SOUND_BUILTIN
#define SOUND_VISUAL_BELL_SCREEN SOUND_CUSTOM

static void set_combox_for_theme_name (GladeXML *dialog, const char *name);
static gboolean theme_changed_custom_reinit (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data);
static void dump_theme (GladeXML *dialog);

static int
visual_bell_gconf_to_setting (GConfClient *client)
{
	char *value;
	int retval;

	if (gconf_client_get_bool (client, VISUAL_BELL_KEY, NULL) == FALSE) {
		return SOUND_OFF;
	}
	value = gconf_client_get_string (client, VISUAL_BELL_TYPE_KEY, NULL);
	if (value == NULL)
		return SOUND_VISUAL_BELL_SCREEN;
	if (strcmp (value, "fullscreen") == 0)
		retval = SOUND_VISUAL_BELL_SCREEN;
	else if (strcmp (value, "frame_flash") == 0)
		retval = SOUND_VISUAL_BELL_TITLEBAR;
	else
		retval = SOUND_VISUAL_BELL_SCREEN;

	g_free (value);

	return retval;
}

static void
visual_bell_setting_to_gconf (GConfClient *client, int setting)
{
	const char *value;

	value = NULL;

	switch (setting) {
	case SOUND_OFF:
		break;
	case SOUND_VISUAL_BELL_SCREEN:
		value = "fullscreen";
		break;
	case SOUND_VISUAL_BELL_TITLEBAR:
		value = "frame_flash";
		break;
	default:
		g_assert_not_reached ();
	}

	gconf_client_set_string (client, VISUAL_BELL_TYPE_KEY, value ? value : "fullscreen", NULL);
	gconf_client_set_bool (client, VISUAL_BELL_KEY, value != NULL, NULL);
}

static void
theme_changed_cb (GConfClient *client,
		  guint cnxn_id,
		  GConfEntry *entry,
		  GladeXML *dialog)
{
	char *theme_name;

	theme_name = gconf_client_get_string (client, SOUND_THEME_KEY, NULL);
	set_combox_for_theme_name (dialog, theme_name);
	g_free (theme_name);
}

static char *
load_index_theme_name (const char *index, char **parent)
{
	GKeyFile *file;
	char *indexname = NULL;
	gboolean hidden;

	file = g_key_file_new ();
	if (g_key_file_load_from_file (file, index, G_KEY_FILE_KEEP_TRANSLATIONS, NULL) == FALSE) {
		g_key_file_free (file);
		return NULL;
	}
	/* Don't add hidden themes to the list */
	hidden = g_key_file_get_boolean (file, "Sound Theme", "Hidden", NULL);
	if (!hidden) {
		indexname = g_key_file_get_locale_string (file,
							  "Sound Theme",
							  "Name",
							  NULL,
							  NULL);

		/* Save the parent theme, if there's one */
		if (parent != NULL) {
			*parent = g_key_file_get_string (file,
							 "Sound Theme",
							 "Inherits",
							 NULL);
		}
	}

	g_key_file_free (file);
	return indexname;
}

static void
add_theme_to_store (const char *key,
		    const char *value,
		    GtkListStore *store)
{
	char *parent;

	parent = NULL;

	/* Get the parent, if we're checking the custom theme */
	if (strcmp (key, CUSTOM_THEME_NAME) == 0) {
		char *name, *path;

		path = custom_theme_dir_path ("index.theme");
		name = load_index_theme_name (path, &parent);
		g_free (name);
		g_free (path);
	}
	gtk_list_store_insert_with_values (store, NULL, G_MAXINT,
					   THEME_DISPLAY_COL, value,
					   THEME_IDENTIFIER_COL, key,
					   THEME_PARENT_ID_COL, parent,
					   -1);
	g_free (parent);
}

static void
sound_theme_in_dir (GHashTable *hash, const char *dir)
{
	GDir *d;
	const char *name;

	d = g_dir_open (dir, 0, NULL);
	if (d == NULL)
		return;
	while ((name = g_dir_read_name (d)) != NULL) {
		char *dirname, *index, *indexname;

		/* Look for directories */
		dirname = g_build_filename (dir, name, NULL);
		if (g_file_test (dirname, G_FILE_TEST_IS_DIR) == FALSE) {
			g_free (dirname);
			continue;
		}

		/* Look for index files */
		index = g_build_filename (dirname, "index.theme", NULL);
		g_free (dirname);

		/* Check the name of the theme in the index.theme file */
		indexname = load_index_theme_name (index, NULL);
		g_free (index);
		if (indexname == NULL)
			continue;

		g_hash_table_insert (hash, g_strdup (name), indexname);
	}

	g_dir_close (d);
}

static void
set_combox_for_theme_name (GladeXML *dialog, const char *name)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	gboolean found;

	g_debug ("setting theme %s", name);

	/* If the name is empty, use "freedesktop" */
	if (name == NULL || *name == '\0')
		name = "freedesktop";

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (WID ("sound_theme_combobox")));

	if (gtk_tree_model_get_iter_first (model, &iter) == FALSE) {
		return;
	}

	do {
		char *value;

		gtk_tree_model_get (model, &iter, THEME_IDENTIFIER_COL, &value, -1);
		found = (value != NULL && strcmp (value, name) == 0);
		g_free (value);

	} while (!found && gtk_tree_model_iter_next (model, &iter));

	/* When we can't find the theme we need to set, try to set the default
	 * one "freedesktop" */
	if (found) {
		gtk_combo_box_set_active_iter (GTK_COMBO_BOX (WID ("sound_theme_combobox")), &iter);
	} else if (strcmp (name, "freedesktop") != 0) {
		g_debug ("not found, falling back to fdo");
		set_combox_for_theme_name (dialog, "freedesktop");
	}
}

static void
theme_combobox_changed (GtkComboBox *widget,
			GladeXML *dialog)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GConfClient *client;
	char *theme_name;

	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (WID ("sound_theme_combobox")), &iter) == FALSE)
		return;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (WID ("sound_theme_combobox")));
	gtk_tree_model_get (model, &iter, THEME_IDENTIFIER_COL, &theme_name, -1);

	client = gconf_client_get_default ();
	gconf_client_set_string (client, SOUND_THEME_KEY, theme_name, NULL);
	g_object_unref (client);

	/* Don't reinit a custom theme */
	if (strcmp (theme_name, CUSTOM_THEME_NAME) != 0) {
		model = gtk_tree_view_get_model (GTK_TREE_VIEW (WID ("sounds_treeview")));
		gtk_tree_model_foreach (model, theme_changed_custom_reinit, NULL);

		/* Delete the custom dir */
		delete_custom_theme_dir ();

		/* And the combo box entry */
		model = gtk_combo_box_get_model (GTK_COMBO_BOX (WID ("sound_theme_combobox")));
		gtk_tree_model_get_iter_first (model, &iter);
		do {
			char *parent;
			gtk_tree_model_get (model, &iter, THEME_PARENT_ID_COL, &parent, -1);
			if (parent != NULL && strcmp (parent, CUSTOM_THEME_NAME) != 0) {
				gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
				g_free (parent);
				break;
			}
			g_free (parent);
		} while (gtk_tree_model_iter_next (model, &iter));
	}
}

void
setup_sound_theme (GladeXML *dialog)
{
	GHashTable *hash;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	const char * const *data_dirs;
	const char *data_dir;
	char *dir, *theme_name;
	GConfClient *client;
	guint i;

	/* Add the theme names and their display name to a hash table,
	 * makes it easy to avoid duplicate themes */
	hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	data_dirs = g_get_system_data_dirs ();
	for (i = 0; data_dirs[i] != NULL; i++) {
		dir = g_build_filename (data_dirs[i], "sounds", NULL);
		sound_theme_in_dir (hash, dir);
		g_free (dir);
	}

	data_dir = g_get_user_data_dir ();
	dir = g_build_filename (data_dir, "sounds", NULL);
	sound_theme_in_dir (hash, dir);
	g_free (dir);

	/* If there isn't at least one theme, make everything
	 * insensitive, LAME! */
	if (g_hash_table_size (hash) == 0) {
		gtk_widget_set_sensitive (WID ("sounds_vbox"), FALSE);
		g_warning ("Bad setup, install the freedesktop sound theme");
		g_hash_table_destroy (hash);
		return;
	}

	/* Setup the tree model, 3 columns:
	 * - internal theme name/directory
	 * - display theme name
	 * - the internal id for the parent theme, used for the custom theme */
	store = gtk_list_store_new (THEME_NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	/* Add the themes to a combobox */
	g_hash_table_foreach (hash, (GHFunc) add_theme_to_store, store);
	g_hash_table_destroy (hash);

	/* Set the display */
	gtk_combo_box_set_model (GTK_COMBO_BOX (WID ("sound_theme_combobox")),
				 GTK_TREE_MODEL (store));

	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (WID ("sound_theme_combobox")),
				    renderer, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (WID ("sound_theme_combobox")),
					renderer, "text", THEME_DISPLAY_COL, NULL);

	/* Setup the default as per the GConf setting */
	client = gconf_client_get_default ();
	theme_name = gconf_client_get_string (client, SOUND_THEME_KEY, NULL);
	set_combox_for_theme_name (dialog, theme_name);
	g_free (theme_name);

	/* Listen for changes in GConf, and on the combobox */
	gconf_client_notify_add (client, SOUND_THEME_KEY,
				 (GConfClientNotifyFunc) theme_changed_cb, dialog, NULL, NULL);
	g_object_unref (client);

	g_signal_connect (WID ("sound_theme_combobox"), "changed",
			  G_CALLBACK (theme_combobox_changed), dialog);
}

enum {
	DISPLAY_COL,
	SETTING_COL,
	TYPE_COL,
	SENSITIVE_COL,
	HAS_PREVIEW_COL,
	FILENAME_COL,
	SOUND_NAMES_COL,
	NUM_COLS
};

static void
setting_set_func (GtkTreeViewColumn *tree_column,
		  GtkCellRenderer   *cell,
		  GtkTreeModel      *model,
		  GtkTreeIter       *iter,
		  gpointer           data)
{
	int setting;
	char *filename;
	SoundType type;

	gtk_tree_model_get (model, iter,
			    SETTING_COL, &setting,
			    FILENAME_COL, &filename,
			    TYPE_COL, &type,
			    -1);

	if (setting == SOUND_UNSET) {
		g_object_set (cell,
			      "visible", FALSE,
			      NULL);
		g_free (filename);
		return;
	}

	if (type != SOUND_TYPE_VISUAL_BELL) {
		if (setting == SOUND_OFF) {
			g_object_set (cell,
				      "text", _("Disabled"),
				      NULL);
		} else if (setting == SOUND_BUILTIN) {
			g_object_set (cell,
				      "text", _("Default"),
				      NULL);
		} else if (setting == SOUND_CUSTOM || setting == SOUND_CUSTOM_OLD) {
			char *display;

			display = g_filename_display_basename (filename);
			g_object_set (cell,
				      "text", display,
				      NULL);
			g_free (display);
		}
	} else {
		if (setting == SOUND_OFF) {
			g_object_set (cell,
				      "text", _("Disabled"),
				      NULL);
		} else if (setting == SOUND_VISUAL_BELL_SCREEN) {
			g_object_set (cell,
				      "text", _("Flash screen"),
				      NULL);
		} else if (setting == SOUND_VISUAL_BELL_TITLEBAR) {
			g_object_set (cell,
				      "text", _("Flash window"),
				      NULL);
		}
	}
	g_free (filename);
}

static void
play_sound_preview (GtkFileChooser *chooser,
		    gpointer user_data)
{
	char *filename;
	ca_context *ctx;

	filename = gtk_file_chooser_get_preview_filename (GTK_FILE_CHOOSER (chooser));
	if (filename == NULL)
		return;

	ctx = ca_gtk_context_get ();
	ca_gtk_play_for_widget (GTK_WIDGET (chooser), 0,
				CA_PROP_APPLICATION_NAME, _("Sound Preferences"),
				CA_PROP_MEDIA_FILENAME, filename,
				CA_PROP_EVENT_DESCRIPTION, _("Testing event sound"),
				CA_PROP_CANBERRA_CACHE_CONTROL, "never",
#ifdef CA_PROP_CANBERRA_ENABLE
				CA_PROP_CANBERRA_ENABLE, "1",
#endif
				NULL);
	g_free (filename);
}

static char *
get_sound_filename (GladeXML *dialog)
{
	GtkWidget *chooser;
	int response;
	char *filename, *path;
	const char * const *data_dirs, *data_dir;
	GtkFileFilter *filter;
	guint i;

	chooser = gtk_file_chooser_dialog_new (_("Select Sound File"),
					       GTK_WINDOW (WID("sound_prefs_dialog")),
					       GTK_FILE_CHOOSER_ACTION_OPEN,
					       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					       GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					       NULL);

	gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);
	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (chooser), FALSE);

	filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (filter, _("Sound files"));
	gtk_file_filter_add_mime_type (filter, "audio/x-vorbis+ogg");
	gtk_file_filter_add_mime_type (filter, "audio/x-wav");
	gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
	gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), filter);

	g_signal_connect (chooser, "update-preview",
			  G_CALLBACK (play_sound_preview), NULL);

	data_dirs = g_get_system_data_dirs ();
	for (i = 0; data_dirs[i] != NULL; i++) {
		path = g_build_filename (data_dirs[i], "sounds", NULL);
		gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (chooser), path, NULL);
		g_free (path);
	}
	data_dir = g_get_user_special_dir (G_USER_DIRECTORY_MUSIC);
	if (data_dir != NULL)
		gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (chooser), data_dir, NULL);

	response = gtk_dialog_run (GTK_DIALOG (chooser));
	filename = NULL;
	if (response == GTK_RESPONSE_ACCEPT)
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));

	gtk_widget_destroy (chooser);

	return filename;
}

static void
fill_custom_model (GtkListStore *store, const char *prev_filename)
{
	GtkTreeIter iter;

	gtk_list_store_clear (store);

	if (prev_filename != NULL) {
		char *display;
		display = g_filename_display_basename (prev_filename);
		gtk_list_store_insert_with_values (store, &iter, G_MAXINT,
						   0, display,
						   1, SOUND_CUSTOM_OLD,
						   -1);
		g_free (display);
	}
	gtk_list_store_insert_with_values (store, &iter, G_MAXINT,
					   0, _("Default"),
					   1, SOUND_BUILTIN,
					   -1);
	gtk_list_store_insert_with_values (store, &iter, G_MAXINT,
					   0, _("Disabled"),
					   1, SOUND_OFF,
					   -1);
	gtk_list_store_insert_with_values (store, &iter, G_MAXINT,
					   0, _("Custom..."),
					   1, SOUND_CUSTOM, -1);
}

static void
fill_visual_bell_model (GtkListStore *store)
{
	GtkTreeIter iter;

	gtk_list_store_clear (store);

	gtk_list_store_insert_with_values (store, &iter, G_MAXINT,
					   0, _("Disabled"),
					   1, SOUND_OFF,
					   -1);
	gtk_list_store_insert_with_values (store, &iter, G_MAXINT,
					   0, _("Flash screen"),
					   1, SOUND_VISUAL_BELL_SCREEN,
					   -1);
	gtk_list_store_insert_with_values (store, &iter, G_MAXINT,
					   0, _("Flash window"),
					   1, SOUND_VISUAL_BELL_TITLEBAR,
					   -1);
}

static void
combobox_editing_started (GtkCellRenderer *renderer,
			  GtkCellEditable *editable,
			  gchar           *path,
			  GladeXML        *dialog)
{
	GtkTreeModel *model, *store;
	GtkTreeIter iter;
	SoundType type;
	char *filename;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (WID ("sounds_treeview")));
	if (gtk_tree_model_get_iter_from_string (model, &iter, path) == FALSE) {
		return;
	}

	gtk_tree_model_get (model, &iter, TYPE_COL, &type, FILENAME_COL, &filename, -1);
	g_object_get (renderer, "model", &store, NULL);
	if (type == SOUND_TYPE_VISUAL_BELL)
		fill_visual_bell_model (GTK_LIST_STORE (store));
	else
		fill_custom_model (GTK_LIST_STORE (store), filename);
	g_free (filename);
}

static gboolean
save_sounds (GtkTreeModel *model,
	     GtkTreePath *path,
	     GtkTreeIter *iter,
	     gpointer data)
{
	int type, setting;
	char *filename, **sounds;

	gtk_tree_model_get (model, iter,
			    TYPE_COL, &type,
			    SETTING_COL, &setting,
			    FILENAME_COL, &filename,
			    SOUND_NAMES_COL, &sounds,
			    -1);
	if (type == SOUND_TYPE_VISUAL_BELL)
		return FALSE;

	if (setting == SOUND_BUILTIN) {
		delete_old_files (sounds);
		delete_disabled_files (sounds);
	} else if (setting == SOUND_OFF) {
		delete_old_files (sounds);
		add_disabled_file (sounds);
	} else if (setting == SOUND_CUSTOM || setting == SOUND_CUSTOM_OLD) {
		delete_old_files (sounds);
		delete_disabled_files (sounds);
		add_custom_file (sounds, filename);
	}
	g_free (filename);
	g_strfreev (sounds);

	return FALSE;
}

static void
save_custom_theme (GtkTreeModel *model, const char *parent)
{
	GKeyFile *keyfile;
	char *data, *path;

	/* Create the custom directory */
	path = custom_theme_dir_path (NULL);
	g_mkdir_with_parents (path, 0755);
	g_free (path);

	/* Save the sounds themselves */
	gtk_tree_model_foreach (model, (GtkTreeModelForeachFunc) save_sounds, NULL);

	/* Set the data for index.theme */
	keyfile = g_key_file_new ();
	g_key_file_set_string (keyfile, "Sound Theme", "Name", _("Custom"));
	g_key_file_set_string (keyfile, "Sound Theme", "Inherits", parent);
	g_key_file_set_string (keyfile, "Sound Theme", "Directories", ".");
	data = g_key_file_to_data (keyfile, NULL, NULL);
	g_key_file_free (keyfile);

	/* Save the index.theme */
	path = custom_theme_dir_path ("index.theme");
	g_file_set_contents (path, data, -1, NULL);
	g_free (path);
	g_free (data);

	custom_theme_update_time ();
}

static gboolean
count_customised_sounds (GtkTreeModel *model,
			 GtkTreePath *path,
			 GtkTreeIter *iter,
			 int *num_custom)
{
	int type, setting;

	gtk_tree_model_get (model, iter, TYPE_COL, &type, SETTING_COL, &setting, -1);
	if (type == SOUND_TYPE_VISUAL_BELL)
		return FALSE;

	if (setting == SOUND_OFF || setting == SOUND_CUSTOM || setting == SOUND_CUSTOM_OLD)
		(*num_custom)++;

	return FALSE;
}

static void
dump_theme (GladeXML *dialog)
{
	int num_custom;
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *parent;

	num_custom = 0;
	model = gtk_tree_view_get_model (GTK_TREE_VIEW (WID ("sounds_treeview")));
	gtk_tree_model_foreach (model, (GtkTreeModelForeachFunc) count_customised_sounds, &num_custom);

	g_debug ("%d customised sounds", num_custom);

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (WID ("sound_theme_combobox")));
	/* Get the current theme's name, and set the parent */
	if (gtk_combo_box_get_active_iter (GTK_COMBO_BOX (WID ("sound_theme_combobox")), &iter) == FALSE)
		return;

	if (num_custom == 0) {
		gtk_tree_model_get (model, &iter, THEME_PARENT_ID_COL, &parent, -1);
		if (parent != NULL) {
			set_combox_for_theme_name (dialog, parent);
			g_free (parent);
		}
		gtk_tree_model_get_iter_first (model, &iter);
		do {
			gtk_tree_model_get (model, &iter, THEME_PARENT_ID_COL, &parent, -1);
			if (parent != NULL && strcmp (parent, CUSTOM_THEME_NAME) != 0) {
				gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
				break;
			}
		} while (gtk_tree_model_iter_next (model, &iter));

		delete_custom_theme_dir ();
	} else {
		gtk_tree_model_get (model, &iter, THEME_IDENTIFIER_COL, &parent, -1);
		if (strcmp (parent, CUSTOM_THEME_NAME) != 0) {
			gtk_list_store_insert_with_values (GTK_LIST_STORE (model), NULL, G_MAXINT,
							   THEME_DISPLAY_COL, _("Custom"),
							   THEME_IDENTIFIER_COL, CUSTOM_THEME_NAME,
							   THEME_PARENT_ID_COL, parent,
							   -1);
		} else {
			g_free (parent);
			gtk_tree_model_get (model, &iter, THEME_PARENT_ID_COL, &parent, -1);
		}

		g_debug ("The parent theme is: %s", parent);
		model = gtk_tree_view_get_model (GTK_TREE_VIEW (WID ("sounds_treeview")));
		save_custom_theme (model, parent);
		g_free (parent);

		set_combox_for_theme_name (dialog, CUSTOM_THEME_NAME);
	}
}

static void
setting_column_edited (GtkCellRendererText *renderer,
		       gchar               *path,
		       gchar               *new_text,
		       GladeXML            *dialog)
{
	GtkTreeModel *model, *tree_model;
	GtkTreeIter iter, tree_iter;
	SoundType type;
	char *text, *old_filename;
	int setting;

	if (new_text == NULL)
		return;

	g_object_get (renderer,
		      "model", &model,
		      NULL);

	tree_model = gtk_tree_view_get_model (GTK_TREE_VIEW (WID ("sounds_treeview")));
	if (gtk_tree_model_get_iter_from_string (tree_model, &tree_iter, path) == FALSE)
		return;

	gtk_tree_model_get (tree_model, &tree_iter,
			    TYPE_COL, &type,
			    FILENAME_COL, &old_filename,
			    -1);

	gtk_tree_model_get_iter_first (model, &iter);
	do {
		gint cmp;

		gtk_tree_model_get (model, &iter, 0, &text, 1, &setting, -1);
		cmp = g_utf8_collate (text, new_text);
		g_free (text);

		if (cmp == 0) {
			if (type == SOUND_TYPE_NORMAL || type == SOUND_TYPE_FEEDBACK || type == SOUND_TYPE_AUDIO_BELL) {

				if (setting == SOUND_CUSTOM || (setting == SOUND_CUSTOM_OLD && old_filename == NULL)) {
					char *filename = get_sound_filename (dialog);
					if (filename == NULL)
						break;
					gtk_tree_store_set (GTK_TREE_STORE (tree_model),
							    &tree_iter,
							    SETTING_COL, setting,
							    HAS_PREVIEW_COL, setting != SOUND_OFF,
							    FILENAME_COL, filename,
							    -1);
					g_free (filename);
				} else if (setting == SOUND_CUSTOM_OLD) {
					gtk_tree_store_set (GTK_TREE_STORE (tree_model),
							    &tree_iter,
							    SETTING_COL, setting,
							    HAS_PREVIEW_COL, setting != SOUND_OFF,
							    FILENAME_COL, old_filename,
							    -1);
				} else {
					gtk_tree_store_set (GTK_TREE_STORE (tree_model),
							    &tree_iter,
							    SETTING_COL, setting,
							    HAS_PREVIEW_COL, setting != SOUND_OFF,
							    -1);
				}

				g_debug ("Something changed, dump theme");
				dump_theme (dialog);

				break;
			} else if (type == SOUND_TYPE_VISUAL_BELL) {
				GConfClient *client;

				client = gconf_client_get_default ();
				visual_bell_setting_to_gconf (client, setting);
				g_object_unref (client);
				gtk_tree_store_set (GTK_TREE_STORE (tree_model),
						    &tree_iter,
						    SETTING_COL, setting,
						    -1);
				break;
			}
			g_assert_not_reached ();
		}
	} while (gtk_tree_model_iter_next (model, &iter));
	g_free (old_filename);
}

/* Functions to toggle whether the Input feedback sounds are editable */
static gboolean
input_feedback_foreach (GtkTreeModel *model,
			GtkTreePath *path,
			GtkTreeIter *iter,
			gpointer data)
{
	int type;
	gboolean enabled = GPOINTER_TO_INT (data);

	gtk_tree_model_get (model, iter, TYPE_COL, &type, -1);
	if (type == SOUND_TYPE_FEEDBACK) {
		gtk_tree_store_set (GTK_TREE_STORE (model), iter,
				    SENSITIVE_COL, enabled,
				    HAS_PREVIEW_COL, enabled,
				    -1);
	}
	return FALSE;
}

static void
set_input_feedback_enabled (GladeXML *dialog, gboolean enabled)
{
	GtkTreeModel *model;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (WID ("sounds_treeview")));
	gtk_tree_model_foreach (model, input_feedback_foreach, GINT_TO_POINTER (enabled));
}

static void
input_feedback_changed_cb (GConfClient *client,
			   guint cnxn_id,
			   GConfEntry *entry,
			   GladeXML *dialog)
{
	gboolean input_feedback_enabled;

	input_feedback_enabled = gconf_client_get_bool (client, INPUT_SOUNDS_KEY, NULL);
	set_input_feedback_enabled (dialog, input_feedback_enabled);
}

/* Functions to toggle whether the audible bell sound is editable */
static gboolean
audible_bell_foreach (GtkTreeModel *model,
		      GtkTreePath *path,
		      GtkTreeIter *iter,
		      gpointer data)
{
	int type;
	gboolean enabled = GPOINTER_TO_INT (data);

	gtk_tree_model_get (model, iter, TYPE_COL, &type, -1);
	if (type == SOUND_TYPE_AUDIO_BELL) {
		gtk_tree_store_set (GTK_TREE_STORE (model), iter,
				    SENSITIVE_COL, enabled,
				    HAS_PREVIEW_COL, enabled,
				    -1);
		return TRUE;
	}
	return FALSE;
}

static void
set_audible_bell_enabled (GladeXML *dialog, gboolean enabled)
{
	GtkTreeModel *model;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (WID ("sounds_treeview")));
	gtk_tree_model_foreach (model, audible_bell_foreach, GINT_TO_POINTER (enabled));
}

static void
audible_bell_changed_cb (GConfClient *client,
			 guint cnxn_id,
			 GConfEntry *entry,
			 GladeXML *dialog)
{
	gboolean audio_bell_enabled;

	audio_bell_enabled = gconf_client_get_bool (client, AUDIO_BELL_KEY, NULL);
	set_audible_bell_enabled (dialog, audio_bell_enabled);
}

static gboolean
theme_changed_custom_reinit (GtkTreeModel *model,
			     GtkTreePath *path,
			     GtkTreeIter *iter,
			     gpointer data)
{
	int type;
	gboolean sensitive;

	gtk_tree_model_get (model, iter,
			    TYPE_COL, &type,
			    SENSITIVE_COL, &sensitive, -1);
	if (type != -1 && type != SOUND_TYPE_VISUAL_BELL) {
		gtk_tree_store_set (GTK_TREE_STORE (model), iter,
				    SETTING_COL, SOUND_BUILTIN,
				    HAS_PREVIEW_COL, sensitive,
				    -1);
	}
	return FALSE;
}

static int
get_file_type (const char *sound_name, char **linked_name)
{
	char *name, *filename;

	*linked_name = NULL;

	name = g_strdup_printf ("%s.disabled", sound_name);
	filename = custom_theme_dir_path (name);
	g_free (name);

	if (g_file_test (filename, G_FILE_TEST_IS_REGULAR) != FALSE) {
		g_free (filename);
		return SOUND_OFF;
	}
	g_free (filename);

	/* We only check for .ogg files because those are the
	 * only ones we create */
	name = g_strdup_printf ("%s.ogg", sound_name);
	filename = custom_theme_dir_path (name);
	g_free (name);

	if (g_file_test (filename, G_FILE_TEST_IS_SYMLINK) != FALSE) {
		*linked_name = g_file_read_link (filename, NULL);
		g_free (filename);
		return SOUND_CUSTOM;
	}
	g_free (filename);

	return SOUND_BUILTIN;
}

static gboolean
theme_changed_custom_init (GtkTreeModel *model,
			   GtkTreePath *path,
			   GtkTreeIter *iter,
			   gpointer data)
{
	char **sound_names;

	gtk_tree_model_get (model, iter, SOUND_NAMES_COL, &sound_names, -1);
	if (sound_names != NULL) {
		char *filename;
		int type;

		type = get_file_type (sound_names[0], &filename);

		gtk_tree_store_set (GTK_TREE_STORE (model), iter,
				    SETTING_COL, type,
				    HAS_PREVIEW_COL, type != SOUND_OFF,
				    FILENAME_COL, filename,
				    -1);
		g_strfreev (sound_names);
		g_free (filename);
	}
	return FALSE;
}

static gboolean
play_sound_at_path (GtkWidget	      *tree_view,
		    GtkTreeViewColumn *column,
		    GtkTreePath       *path)
{
	GObject *preview_column;

	preview_column = g_object_get_data (G_OBJECT (tree_view), "preview-column");
	if (column == (GtkTreeViewColumn *) preview_column) {
		GtkTreeModel *model;
		GtkTreeIter iter;
		char **sound_names;
		gboolean sensitive;
		ca_context *ctx;

		model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
		if (gtk_tree_model_get_iter (model, &iter, path) == FALSE) {
			return FALSE;
		}

		gtk_tree_model_get (model, &iter,
				    SOUND_NAMES_COL, &sound_names,
				    SENSITIVE_COL, &sensitive, -1);
		if (!sensitive || sound_names == NULL)
			return FALSE;

		ctx = ca_gtk_context_get ();
		ca_gtk_play_for_widget (GTK_WIDGET (tree_view), 0,
					CA_PROP_APPLICATION_NAME, _("Sound Preferences"),
					CA_PROP_EVENT_ID, sound_names[0],
					CA_PROP_EVENT_DESCRIPTION, _("Testing event sound"),
					CA_PROP_CANBERRA_CACHE_CONTROL, "never",
#ifdef CA_PROP_CANBERRA_ENABLE
					CA_PROP_CANBERRA_ENABLE, "1",
#endif
					NULL);

		g_strfreev (sound_names);

		return TRUE;
	}
	return FALSE;
}

static gboolean
custom_treeview_button_press_event_cb (GtkWidget *tree_view,
				       GdkEventButton *event,
				       GladeXML *dialog)
{
	GtkTreePath *path;
	GtkTreeViewColumn *column;
	GdkEventButton *button_event = (GdkEventButton *) event;
	gboolean res = FALSE;

	if (event->type != GDK_BUTTON_PRESS)
		return TRUE;

	if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (tree_view),
					   button_event->x, button_event->y,
					   &path, &column, NULL, NULL)) {
		res = play_sound_at_path (tree_view, column, path);
		gtk_tree_path_free (path);
	}

	return res;
}

typedef GtkCellRendererPixbuf      ActivatableCellRendererPixbuf;
typedef GtkCellRendererPixbufClass ActivatableCellRendererPixbufClass;

#define ACTIVATABLE_TYPE_CELL_RENDERER_PIXBUF (activatable_cell_renderer_pixbuf_get_type ())
G_DEFINE_TYPE (ActivatableCellRendererPixbuf, activatable_cell_renderer_pixbuf, GTK_TYPE_CELL_RENDERER_PIXBUF);

static gboolean
activatable_cell_renderer_pixbuf_activate (GtkCellRenderer      *cell,
                                 	   GdkEvent             *event,
                                   	   GtkWidget            *widget,
                                   	   const gchar          *path_string,
                                   	   GdkRectangle         *background_area,
                                   	   GdkRectangle         *cell_area,
                                   	   GtkCellRendererState  flags)
{
	GtkTreeViewColumn *preview_column;
	GtkTreePath *path;
	gboolean res;

        preview_column = g_object_get_data (G_OBJECT (widget), "preview-column");
	path = gtk_tree_path_new_from_string (path_string);
	res = play_sound_at_path (widget, preview_column, path);
	gtk_tree_path_free (path);

	return res;
}

static void
activatable_cell_renderer_pixbuf_init (ActivatableCellRendererPixbuf *cell)
{
}

static void
activatable_cell_renderer_pixbuf_class_init (ActivatableCellRendererPixbufClass *class)
{
  GtkCellRendererClass *cell_class;

  cell_class = GTK_CELL_RENDERER_CLASS (class);

  cell_class->activate = activatable_cell_renderer_pixbuf_activate;
}


void
setup_sound_theme_custom (GladeXML *dialog, gboolean have_xkb)
{
	GtkTreeStore *store;
	GtkTreeModel *custom_model;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkTreeIter iter, parent;
	GConfClient *client;
	CategoryType type;
	gboolean input_feedback_enabled, audible_bell_enabled;
	int visual_bell_setting;
	char *theme_name;
	guint i;

	client = gconf_client_get_default ();
	visual_bell_setting = visual_bell_gconf_to_setting (client);

	/* Set up the model for the custom view */
	store = gtk_tree_store_new (NUM_COLS, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRV);

	/* The first column with the categories/sound names */
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Display", renderer,
							   "text", DISPLAY_COL,
							   "sensitive", SENSITIVE_COL,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (WID ("sounds_treeview")), column);

	/* The 2nd column with the sound settings */
	renderer = gtk_cell_renderer_combo_new ();
	g_signal_connect (renderer, "edited", G_CALLBACK (setting_column_edited), dialog);
	g_signal_connect (renderer, "editing-started", G_CALLBACK (combobox_editing_started), dialog);
	custom_model = GTK_TREE_MODEL (gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT));
	fill_custom_model (GTK_LIST_STORE (custom_model), NULL);
	g_object_set (renderer, "model", custom_model, "has-entry", FALSE, "editable", TRUE, "text-column", 0, NULL);
	column = gtk_tree_view_column_new_with_attributes ("Setting", renderer,
							   "editable", SENSITIVE_COL,
							   "sensitive", SENSITIVE_COL,
							   "visible", TRUE,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (WID ("sounds_treeview")), column);
	gtk_tree_view_column_set_cell_data_func (column, renderer, setting_set_func, NULL, NULL);

	/* The 3rd column with the preview pixbuf */
	renderer = g_object_new (ACTIVATABLE_TYPE_CELL_RENDERER_PIXBUF, NULL);
	g_object_set (renderer,
		      "mode", GTK_CELL_RENDERER_MODE_ACTIVATABLE,
		      "icon-name", "media-playback-start",
		      "stock-size", GTK_ICON_SIZE_MENU,
		      NULL);
	column = gtk_tree_view_column_new_with_attributes ("Preview", renderer,
							   "visible", HAS_PREVIEW_COL,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (WID ("sounds_treeview")), column);
	g_object_set_data (G_OBJECT (WID ("sounds_treeview")), "preview-column", column);

	gtk_tree_view_set_model (GTK_TREE_VIEW (WID ("sounds_treeview")), GTK_TREE_MODEL (store));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (WID ("sounds_treeview")), FALSE);

	g_signal_connect (WID ("sounds_treeview"), "button-press-event",
			  G_CALLBACK (custom_treeview_button_press_event_cb), dialog);

	/* Fill in the model */
	type = CATEGORY_INVALID;

	for (i = 0; ; i++) {
		GtkTreeIter *_parent;

		if (sounds[i].category == -1)
			break;

		if (sounds[i].type == SOUND_TYPE_VISUAL_BELL && have_xkb == FALSE)
			continue;

		/* Is it a new type of sound? */
		if (sounds[i].category == type && type != CATEGORY_INVALID && type != CATEGORY_BELL)
			_parent = &parent;
		else
			_parent = NULL;

		if (sounds[i].type == SOUND_TYPE_VISUAL_BELL)
			gtk_tree_store_insert_with_values (store, &iter, _parent, G_MAXINT,
							   DISPLAY_COL, _(sounds[i].display_name),
							   SETTING_COL, visual_bell_setting,
							   TYPE_COL, sounds[i].type,
							   HAS_PREVIEW_COL, FALSE,
							   SENSITIVE_COL, TRUE,
							   -1);
		else if (sounds[i].type != -1)
			gtk_tree_store_insert_with_values (store, &iter, _parent, G_MAXINT,
							   DISPLAY_COL, _(sounds[i].display_name),
							   SETTING_COL, SOUND_BUILTIN,
							   TYPE_COL, sounds[i].type,
							   SOUND_NAMES_COL, sounds[i].names,
							   HAS_PREVIEW_COL, TRUE,
							   SENSITIVE_COL, TRUE,
							   -1);
		else
			/* Category */
			gtk_tree_store_insert_with_values (store, &iter, _parent, G_MAXINT,
							   DISPLAY_COL, _(sounds[i].display_name),
							   SETTING_COL, SOUND_UNSET,
							   TYPE_COL, sounds[i].type,
							   SENSITIVE_COL, TRUE,
							   HAS_PREVIEW_COL, FALSE,
							   -1);

		/* If we didn't set a parent already, set one in case we need it later */
		if (_parent == NULL)
			parent = iter;
		type = sounds[i].category;
	}

	gtk_tree_view_expand_all (GTK_TREE_VIEW (WID ("sounds_treeview")));

	/* Listen to GConf for a bunch of keys */
	input_feedback_enabled = gconf_client_get_bool (client, INPUT_SOUNDS_KEY, NULL);
	if (input_feedback_enabled == FALSE)
		set_input_feedback_enabled (dialog, FALSE);
	gconf_client_notify_add (client, INPUT_SOUNDS_KEY,
				 (GConfClientNotifyFunc) input_feedback_changed_cb, dialog, NULL, NULL);

	audible_bell_enabled = gconf_client_get_bool (client, AUDIO_BELL_KEY, NULL);
	if (audible_bell_enabled == FALSE)
		set_audible_bell_enabled (dialog, FALSE);
	gconf_client_notify_add (client, AUDIO_BELL_KEY,
				 (GConfClientNotifyFunc) audible_bell_changed_cb, dialog, NULL, NULL);

	/* Setup the default values if we're using the custom theme */
	theme_name = gconf_client_get_string (client, SOUND_THEME_KEY, NULL);
	if (theme_name != NULL && strcmp (theme_name, CUSTOM_THEME_NAME) == 0)
		gtk_tree_model_foreach (GTK_TREE_MODEL (store), theme_changed_custom_init, NULL);
	g_free (theme_name);

	g_object_unref (client);
}

