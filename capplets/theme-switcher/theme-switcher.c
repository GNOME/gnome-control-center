/* This program was written with lots of love under the GPL by Jonathan
 * Blandford <jrb@gnome.org>
 */

#include <config.h>

#include <string.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-ops.h>

#include "theme-common.h"
#include "capplet-util.h"
#include "activate-settings-daemon.h"
#include "gconf-property-editor.h"
#include "file-transfer-dialog.h"

#define GTK_THEME_KEY "/desktop/gnome/interface/gtk_theme"

#define MAX_ELEMENTS_BEFORE_SCROLLING 8

enum
{
  THEME_NAME_COLUMN,
  N_COLUMNS
};

enum
{
  TARGET_URI_LIST,
  TARGET_NS_URL
};

static GtkTargetEntry drop_types[] =
{
  {"text/uri-list", 0, TARGET_URI_LIST},
  {"_NETSCAPE_URL", 0, TARGET_NS_URL}
};

static gint n_drop_types = sizeof (drop_types) / sizeof (GtkTargetEntry);
static gboolean setting_model = FALSE;

static GladeXML *
create_dialog (void)
{
  GladeXML *dialog;

  dialog = glade_xml_new (GLADEDIR "/theme-properties.glade", NULL, NULL);

  return dialog;
}

static void
theme_selection_changed (GtkTreeSelection *selection,
			 gpointer          data)
{
  GtkTreeModel *model;
  gchar *new_key;
  GConfClient *client;
  GtkTreeIter iter;

  if (setting_model)
    return;

  client = gconf_client_get_default ();

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
			  THEME_NAME_COLUMN, &new_key,
			  -1);
    }
  else
    /* This shouldn't happen */
    {
      new_key = NULL;
    }

  if (new_key != NULL)
    {
      gchar *old_key;

      old_key = gconf_client_get_string (client, GTK_THEME_KEY, NULL);
      if (old_key && strcmp (old_key, new_key))
	{
	  gconf_client_set_string (client, GTK_THEME_KEY, new_key, NULL);
	}
      g_free (old_key);
    }
  else
    {
      gconf_client_unset (client, GTK_THEME_KEY, NULL);
    }
  g_free (new_key);
}

static void
read_themes (GladeXML *dialog)
{
  GConfClient *client;
  GList *gtk_theme_list;
  GList *list;
  GtkTreeModel *model;
  gchar *current_theme;
  gint i = 0;
  gboolean current_theme_found = FALSE;

  client = gconf_client_get_default ();

  gtk_theme_list = theme_common_get_list ();
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (WID ("theme_treeview")));

  setting_model = TRUE;
  gtk_list_store_clear (GTK_LIST_STORE (model));

  current_theme = gconf_client_get_string (client, GTK_THEME_KEY, NULL);
  if (current_theme == NULL)
    current_theme = g_strdup ("Default");
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (WID ("theme_swindow")),
				  GTK_POLICY_NEVER, GTK_POLICY_NEVER);
  gtk_widget_set_usize (WID ("theme_swindow"), -1, -1);

  for (list = gtk_theme_list; list; list = list->next)
    {
      ThemeInfo *info = list->data;
      GtkTreeIter iter;

      if (! info->has_gtk)
	continue;

      gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			  THEME_NAME_COLUMN, info->name,
			  -1);

      if (strcmp (current_theme, info->name) == 0)
	{
	  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (WID ("theme_treeview")));
	  gtk_tree_selection_select_iter (selection, &iter);
	  current_theme_found = TRUE;
	}

      if (i == MAX_ELEMENTS_BEFORE_SCROLLING)
	{
	  GtkRequisition rectangle;
	  gtk_widget_size_request (WID ("theme_treeview"), &rectangle);
	  gtk_widget_set_usize (WID ("theme_swindow"), -1, rectangle.height);
	  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (WID ("theme_swindow")),
					  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	}
      i++;
    }

  if (! current_theme_found)
    {
      GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (WID ("theme_treeview")));
      GtkTreeIter iter;

      gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			  THEME_NAME_COLUMN, current_theme,
			  -1);
      gtk_tree_selection_select_iter (selection, &iter);
    }
  setting_model = FALSE;

  g_free (current_theme);
}

static void
theme_key_changed (GConfClient *client,
		   guint        cnxn_id,
		   GConfEntry  *entry,
		   gpointer     user_data)
{
  if (strcmp (entry->key, GTK_THEME_KEY))
    return;

  read_themes ((GladeXML *)user_data);
}


static void
theme_changed_func (gpointer uri,
		    gpointer user_data)
{
  read_themes ((GladeXML *)user_data);
}

static gint
sort_func (GtkTreeModel *model,
	   GtkTreeIter  *a,
	   GtkTreeIter  *b,
	   gpointer      user_data)
{
  gchar *a_str = NULL;
  gchar *b_str = NULL;

 gtk_tree_model_get (model, a, 0, &a_str, -1);
 gtk_tree_model_get (model, b, 0, &b_str, -1);

 if (a_str == NULL) a_str = g_strdup ("");
 if (b_str == NULL) b_str = g_strdup ("");

 g_print ("comparing %s to %s\n", a_str, b_str);
 if (!strcmp (a_str, "Default"))
   return -1;
 if (!strcmp (b_str, "Default"))
   return 1;

 return g_utf8_collate (a_str, b_str);
}

/* Callback issued during drag movements */

static gboolean
drag_motion_cb (GtkWidget *widget, GdkDragContext *context,
		gint x, gint y, guint time, gpointer data)
{
	return FALSE;
}

/* Callback issued during drag leaves */

static void
drag_leave_cb (GtkWidget *widget, GdkDragContext *context,
	       guint time, gpointer data)
{
	gtk_widget_queue_draw (widget);
}

/* Callback issued on actual drops. Attempts to load the file dropped. */
static void
drag_data_received_cb (GtkWidget *widget, GdkDragContext *context,
		       gint x, gint y,
		       GtkSelectionData *selection_data,
		       guint info, guint time, gpointer data)
{
	GList *uris;
	GladeXML *dialog = data;
	gchar *filename;

	if (!(info == TARGET_URI_LIST || info == TARGET_NS_URL))
		return;

	uris = gnome_vfs_uri_list_parse ((gchar *) selection_data->data);

	filename = gnome_vfs_uri_to_string (uris->data, GNOME_VFS_URI_HIDE_NONE);
	if (strncmp (filename, "http://", 7) && strncmp (filename, "ftp://", 6))
	{
		g_free (filename);
		filename = gnome_vfs_uri_to_string (uris->data, GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD);
	}
	gnome_file_entry_set_filename (GNOME_FILE_ENTRY (WID ("install_theme_picker")), filename);
	g_free (filename);
	gnome_vfs_uri_list_unref (uris);
	gtk_widget_show (WID ("install_dialog"));
}

static void
show_install_dialog (GtkWidget *button, gpointer data)
{
	GladeXML *dialog = data;
	gtk_widget_show (WID ("install_dialog"));
}

static void
show_manage_themes (GtkWidget *button, gpointer data)
{
	gchar *command = g_strdup_printf ("nautilus --no-desktop %s/.themes",
					  g_get_home_dir ());
	g_spawn_command_line_async (command, NULL);
	g_free (command);
}

static void
transfer_cancel_cb (GtkWidget *dlg, gchar *path)
{
	gnome_vfs_unlink (path);
	g_free (path);
	gtk_widget_destroy (dlg);
}

static void
transfer_done_cb (GtkWidget *dlg, gchar *path)
{
	int len = strlen (path);
	if (path && len > 7 && !strcmp (path + len - 7, ".tar.gz"))
	{
		int status;
		gchar *command;
		/* this should be something more clever and nonblocking */
		command = g_strdup_printf ("sh -c 'gzip -d -c < \"%s\" | tar xf - -C \"%s/.themes\"'",
					   path, g_get_home_dir ());
		g_print ("untarring %s\n", command);
		if (g_spawn_command_line_sync (command, NULL, NULL, &status, NULL) && status == 0)
			gnome_vfs_unlink (path);
		g_free (command);
	}
	g_free (path);
	gtk_widget_destroy (dlg);
}

static void
install_dialog_response (GtkWidget *widget, int response_id, gpointer data)
{
	GladeXML *dialog = data;
	GtkWidget *dlg;
	gchar *filename, *path, *base;
	GList *src, *target;
	GnomeVFSURI *src_uri;
	const gchar *raw;
	
	gtk_widget_hide (widget);
	
	switch (response_id)
	{
	case 0:
		raw = gtk_entry_get_text (GTK_ENTRY (gnome_file_entry_gtk_entry (GNOME_FILE_ENTRY (WID ("install_theme_picker")))));
		if (strncmp (raw, "http://", 7) && strncmp (raw, "ftp://", 6) && *raw != '/')
			filename = gnome_file_entry_get_full_path (GNOME_FILE_ENTRY (WID ("install_theme_picker")), TRUE);
		else
			filename = g_strdup (raw);

		src_uri = gnome_vfs_uri_new (filename);
		base = gnome_vfs_uri_extract_short_name (src_uri);
		src = g_list_append (NULL, src_uri);
		path = g_build_filename (g_get_home_dir (), ".themes",
				         base, NULL);
		target = g_list_append (NULL, gnome_vfs_uri_new (path));
		
		dlg = file_transfer_dialog_new ();
		file_transfer_dialog_wrap_async_xfer (FILE_TRANSFER_DIALOG (dlg),
						      src, target,
						      GNOME_VFS_XFER_RECURSIVE,
						      GNOME_VFS_XFER_ERROR_MODE_QUERY,
						      GNOME_VFS_XFER_OVERWRITE_MODE_QUERY,
						      GNOME_VFS_PRIORITY_DEFAULT);
		gnome_vfs_uri_list_unref (src);
		gnome_vfs_uri_list_unref (target);
		g_free (base);
		g_free (filename);
		g_signal_connect (G_OBJECT (dlg), "cancel",
				  G_CALLBACK (transfer_cancel_cb), path);
		g_signal_connect (G_OBJECT (dlg), "done",
				  G_CALLBACK (transfer_done_cb), path);
		gtk_widget_show (dlg);
		break;
	default:
		break;
	}
}

static void
setup_dialog (GladeXML *dialog)
{
  GConfClient *client;
  GtkWidget *widget;
  GtkTreeModel *model;
  GtkTreeSelection *selection;

  client = gconf_client_get_default ();

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (WID ("theme_treeview")),
					       -1, NULL,
					       gtk_cell_renderer_text_new (),
					       "text", THEME_NAME_COLUMN,
					       NULL);
  gconf_client_add_dir (client, "/desktop/gnome/interface", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  gconf_client_notify_add (client,
			   GTK_THEME_KEY,
			   (GConfClientNotifyFunc) &theme_key_changed,
			   dialog, NULL, NULL);

  model = (GtkTreeModel *) gtk_list_store_new (N_COLUMNS, G_TYPE_STRING);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (model), 0, sort_func, NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model), 0, GTK_SORT_ASCENDING);
  gtk_tree_view_set_model (GTK_TREE_VIEW (WID ("theme_treeview")), model);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (WID ("theme_treeview")));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
  g_signal_connect (G_OBJECT (selection), "changed", (GCallback) theme_selection_changed, NULL);

  read_themes (dialog);
  theme_common_register_theme_change (theme_changed_func, dialog);

  widget = WID ("install_button");
  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (show_install_dialog), dialog);
  widget = WID ("manage_button");
  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (show_manage_themes), dialog);

  widget = WID ("install_dialog");
  g_signal_connect (G_OBJECT (widget), "response",
		    G_CALLBACK (install_dialog_response), dialog);
  
  widget = WID ("theme_dialog");

  g_signal_connect (G_OBJECT (widget), "response", gtk_main_quit, NULL);
  g_signal_connect (G_OBJECT (widget), "close", gtk_main_quit, NULL);

  gtk_drag_dest_set (widget, GTK_DEST_DEFAULT_ALL,
		     drop_types, n_drop_types,
		     GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_MOVE);

  g_signal_connect (G_OBJECT (widget), "drag-motion",
		    G_CALLBACK (drag_motion_cb), NULL);
  g_signal_connect (G_OBJECT (widget), "drag-leave",
		    G_CALLBACK (drag_leave_cb), NULL);
  g_signal_connect (G_OBJECT (widget), "drag-data-received",
		    G_CALLBACK (drag_data_received_cb),
		    dialog);

  gtk_widget_show (widget);
}

int
main (int argc, char *argv[])
{
  GladeXML *dialog;

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gnome_program_init (argv[0], VERSION, LIBGNOMEUI_MODULE, argc, argv,
		      GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
		      NULL);

  activate_settings_daemon ();

  dialog = create_dialog ();
  setup_dialog (dialog);

  gtk_main ();

  return 0;
}
