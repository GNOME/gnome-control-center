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
#include <libgnomevfs/gnome-vfs-utils.h>

#include <libwindow-settings/gnome-wm-manager.h>

#include "gnome-theme-info.h"
#include "capplet-util.h"
#include "activate-settings-daemon.h"
#include "gconf-property-editor.h"
#include "file-transfer-dialog.h"
#include "gnome-theme-installer.h"

#define GTK_THEME_KEY      "/desktop/gnome/interface/gtk_theme"
#define WINDOW_THEME_KEY   "/desktop/gnome/applications/window_manager/theme"
#define ICON_THEME_KEY     "/desktop/gnome/interface/icon_theme"
#define METACITY_THEME_DIR "/apps/metacity/general"
#define METACITY_THEME_KEY METACITY_THEME_DIR "/theme"

#define META_THEME_DEFAULT_NAME   "Default"
#define GTK_THEME_DEFAULT_NAME    "Default"
#define WINDOW_THEME_DEFAULT_NAME "Atlanta"
#define ICON_THEME_DEFAULT_NAME   "Default"

#define MAX_ELEMENTS_BEFORE_SCROLLING 8

static void read_themes (GladeXML *dialog);

enum
{
  THEME_NAME_COLUMN,
  THEME_ID_COLUMN,
  DEFAULT_THEME_COLUMN,
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
static gboolean initial_scroll = TRUE;

static GladeXML *
create_dialog (void)
{
  GladeXML *dialog;

  dialog = glade_xml_new (GLADEDIR "/theme-properties.glade", NULL, NULL);

  return dialog;
}


static void
load_theme_names (GtkTreeView *tree_view,
		  GList       *theme_list,
		  char        *current_theme,
		  gchar       *default_theme)
{
  GList *list;
  GtkTreeModel *model;
  GtkWidget *swindow;
  gint i = 0;
  gboolean current_theme_found = FALSE;
  GtkTreeRowReference *row_ref = NULL;

  swindow = GTK_WIDGET (tree_view)->parent;
  model = gtk_tree_view_get_model (tree_view);

  setting_model = TRUE;
  gtk_list_store_clear (GTK_LIST_STORE (model));
  
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
				  GTK_POLICY_NEVER, GTK_POLICY_NEVER);
  gtk_widget_set_usize (swindow, -1, -1);

  for (list = theme_list; list; list = list->next)
    {
      const char *name = list->data;
      GtkTreeIter iter;
      gboolean is_default;

      gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);

      if (strcmp (default_theme, name) == 0)
	is_default = TRUE;
      else
	is_default = FALSE;

      if (strcmp (current_theme, name) == 0)
	{
	  GtkTreePath *path = gtk_tree_model_get_path (model, &iter);
	  row_ref = gtk_tree_row_reference_new (model, path);
	  gtk_tree_path_free (path);
	  current_theme_found = TRUE;
	}
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			  THEME_NAME_COLUMN, name,
			  DEFAULT_THEME_COLUMN, is_default,
			  -1);

      if (i == MAX_ELEMENTS_BEFORE_SCROLLING)
	{
	  GtkRequisition rectangle;
	  gtk_widget_size_request (GTK_WIDGET (tree_view), &rectangle);
	  gtk_widget_set_usize (swindow, -1, rectangle.height);
	  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
					  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	}
      i++;
    }

  if (! current_theme_found)
    {
      GtkTreeIter iter;
      GtkTreePath *path;
      gboolean is_default;

      if (strcmp (default_theme, current_theme) == 0)
	is_default = TRUE;
      else
	is_default = FALSE;
      gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			  THEME_NAME_COLUMN, current_theme,
			  DEFAULT_THEME_COLUMN, is_default,
			  -1);

      path = gtk_tree_model_get_path (model, &iter);
      row_ref = gtk_tree_row_reference_new (model, path);
      gtk_tree_path_free (path);
    }

  if (row_ref)
    {
      GtkTreePath *path;

      path = gtk_tree_row_reference_get_path (row_ref);
      gtk_tree_view_set_cursor (tree_view,path, NULL, FALSE);

      if (initial_scroll)
	{
	  gtk_tree_view_scroll_to_cell (tree_view, path, NULL, TRUE, 0.5, 0.0);
	  initial_scroll = FALSE;
	}
      
      gtk_tree_path_free (path);
      gtk_tree_row_reference_free (row_ref);
    }
  setting_model = FALSE;
}

/* Shared by icons and gtk+ */
static void
update_gconf_key_from_selection (GtkTreeSelection *selection,
				 const gchar      *gconf_key)
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

      old_key = gconf_client_get_string (client, gconf_key, NULL);
      if (old_key && strcmp (old_key, new_key))
	{
	  gconf_client_set_string (client, gconf_key, new_key, NULL);
	}
      g_free (old_key);
    }
  else
    {
      gconf_client_unset (client, gconf_key, NULL);
    }
  g_free (new_key);
  g_object_unref (client);
}

static void
meta_theme_setup_info (GnomeThemeMetaInfo *meta_theme_info,
		       GladeXML           *dialog)
{
  if (meta_theme_info == NULL)
    {
      gtk_widget_hide (WID ("meta_theme_extras_vbox"));
      gtk_widget_hide (WID ("meta_theme_description_label"));
      gtk_image_set_from_pixbuf (GTK_IMAGE (WID ("meta_theme_image")), NULL);
    }
  else
    {
      if (meta_theme_info->icon_file)
	{
	  gtk_image_set_from_file (GTK_IMAGE (WID ("meta_theme_image")), meta_theme_info->icon_file);
	}
      else
	{
	  gtk_image_set_from_pixbuf (GTK_IMAGE (WID ("meta_theme_image")), NULL);
	}
      if (meta_theme_info->comment)
	{
	  gchar *real_comment;

	  real_comment = g_strconcat ("<span size=\"larger\" weight=\"bold\">",
				      meta_theme_info->comment,
				      "</span>", NULL);
	  gtk_label_set_markup (GTK_LABEL (WID ("meta_theme_description_label")),
				real_comment);
	  g_free (real_comment);
	  gtk_widget_show (WID ("meta_theme_description_label"));
	}
      else
	{
	  gtk_widget_hide (WID ("meta_theme_description_label"));
	}

      if (meta_theme_info->font != NULL)
	{
	  gtk_widget_show (WID ("meta_theme_extras_vbox"));
	  if (meta_theme_info->background != NULL)
	    {
	      gtk_label_set_text (GTK_LABEL (WID ("meta_theme_info_label")),
				  _("This theme suggests the use of a font and a background:"));
	      gtk_widget_show (WID ("meta_theme_background_button"));
	      gtk_widget_show (WID ("meta_theme_font_button"));
	    }
	  else
	    {
	      gtk_label_set_text (GTK_LABEL (WID ("meta_theme_info_label")),
				  _("This theme suggests the use of a font:"));
	      gtk_widget_hide (WID ("meta_theme_background_button"));
	      gtk_widget_show (WID ("meta_theme_font_button"));
	    }
	}
      else
	{
	  if (meta_theme_info->background != NULL)
	    {
	      gtk_widget_show (WID ("meta_theme_extras_vbox"));
	      gtk_label_set_text (GTK_LABEL (WID ("meta_theme_info_label")),
				  _("This theme suggests the use of a background:"));
	      gtk_widget_show (WID ("meta_theme_background_button"));
	      gtk_widget_hide (WID ("meta_theme_font_button"));
	    }
	  else
	    {
	      gtk_widget_hide (WID ("meta_theme_extras_vbox"));
	      gtk_widget_hide (WID ("meta_theme_background_button"));
	      gtk_widget_hide (WID ("meta_theme_font_button"));
	    }
	}
    }
}

static void
meta_theme_set (GnomeThemeMetaInfo *meta_theme_info)
{
  GConfClient *client;
  gchar *old_key;
  GnomeWindowManager *window_manager;
  GnomeWMSettings wm_settings;

  window_manager = gnome_wm_manager_get_current (gdk_display_get_default_screen (gdk_display_get_default ()));

  client = gconf_client_get_default ();

  /* Set the gtk+ key */
  old_key = gconf_client_get_string (client, GTK_THEME_KEY, NULL);
  if (old_key && strcmp (old_key, meta_theme_info->gtk_theme_name))
    {
      gconf_client_set_string (client, GTK_THEME_KEY, meta_theme_info->gtk_theme_name, NULL);
    }
  g_free (old_key);

  /* Set the wm key */
  wm_settings.flags = GNOME_WM_SETTING_THEME;
  wm_settings.theme = meta_theme_info->metacity_theme_name;
  gnome_window_manager_change_settings (window_manager, &wm_settings);

  /* set the icon theme */
  old_key = gconf_client_get_string (client, ICON_THEME_KEY, NULL);
  if (old_key && strcmp (old_key, meta_theme_info->icon_theme_name))
    {
      gconf_client_set_string (client, ICON_THEME_KEY, meta_theme_info->icon_theme_name, NULL);
    }
  g_free (old_key);
  
}

static void
meta_theme_selection_changed (GtkTreeSelection *selection,
			      GladeXML         *dialog)
{
  GnomeThemeMetaInfo *meta_theme_info;
  GtkTreeIter iter;
  gchar *meta_theme_name;
  GtkTreeModel *model;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
			  THEME_NAME_COLUMN, &meta_theme_name,
			  -1);
    }
  else
    /* This shouldn't happen */
    return;

  meta_theme_info = gnome_theme_meta_info_find (meta_theme_name);
  meta_theme_setup_info (meta_theme_info, dialog);

  if (setting_model)
    return;

  if (meta_theme_info)
    meta_theme_set (meta_theme_info);
}

static void
gtk_theme_selection_changed (GtkTreeSelection *selection,
			     gpointer          data)
{
  update_gconf_key_from_selection (selection, GTK_THEME_KEY);
}

static void
window_theme_selection_changed (GtkTreeSelection *selection,
				gpointer          data)
{
  GnomeWindowManager *window_manager;
  GnomeWMSettings wm_settings;
  GtkTreeIter iter;
  gchar *window_theme_name;
  GtkTreeModel *model;

  if (setting_model)
    return;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
			  THEME_NAME_COLUMN, &window_theme_name,
			  -1);
    }
  else
    /* This shouldn't happen */
    return;

  window_manager = gnome_wm_manager_get_current (gdk_display_get_default_screen (gdk_display_get_default ()));

  wm_settings.flags = GNOME_WM_SETTING_THEME;
  wm_settings.theme = window_theme_name;
  gnome_window_manager_change_settings (window_manager, &wm_settings);

}

static void
icon_theme_selection_changed (GtkTreeSelection *selection,
			     gpointer          data)
{
  update_gconf_key_from_selection (selection, ICON_THEME_KEY);
}


/* This function will adjust the UI to reflect the current theme/gconf
 * situation.  It is called after the themes change on disk, or a gconf-key
 * changes.
 */
static void
read_themes (GladeXML *dialog)
{
  GList *theme_list;
  GList *string_list;
  GList *list;
  GConfClient *client;
  gchar *current_meta_theme;
  gchar *current_gtk_theme;
  gchar *current_window_theme;
  gchar *current_icon_theme;
  GnomeWindowManager *window_manager;
  GnomeWMSettings wm_settings;

  client = gconf_client_get_default ();

  current_meta_theme = NULL;
  current_gtk_theme = gconf_client_get_string (client, GTK_THEME_KEY, NULL);
  current_icon_theme = gconf_client_get_string (client, ICON_THEME_KEY, NULL);
  window_manager = gnome_wm_manager_get_current (gdk_display_get_default_screen (gdk_display_get_default ()));
  wm_settings.flags = GNOME_WM_SETTING_THEME;
  gnome_window_manager_get_settings (window_manager, &wm_settings);
  current_window_theme = g_strdup (wm_settings.theme);

  if (current_icon_theme == NULL)
    current_icon_theme = g_strdup ("Default");
  if (current_gtk_theme == NULL)
    current_gtk_theme = g_strdup ("Default");

  /* First, we update the GTK+ themes page */
  theme_list = gnome_theme_info_find_by_type (GNOME_THEME_GTK_2);
  string_list = NULL;
  for (list = theme_list; list; list = list->next)
    {
      GnomeThemeInfo *info = list->data;
      string_list = g_list_prepend (string_list, info->name);
    }

  load_theme_names (GTK_TREE_VIEW (WID ("control_theme_treeview")), string_list, current_gtk_theme, GTK_THEME_DEFAULT_NAME);
  g_list_free (string_list);
  g_list_free (theme_list);

  /* Next, we do the window managers */
  string_list = gnome_window_manager_get_theme_list (window_manager);
  load_theme_names (GTK_TREE_VIEW (WID ("window_theme_treeview")), string_list, current_window_theme, WINDOW_THEME_DEFAULT_NAME);
  g_list_free (string_list);

  /* Third, we do the icon theme */
  theme_list = gnome_theme_icon_info_find_all ();
  string_list = NULL;

  for (list = theme_list; list; list = list->next)
    {
      GnomeThemeIconInfo *info = list->data;
      string_list = g_list_prepend (string_list, info->name);
    }

  load_theme_names (GTK_TREE_VIEW (WID ("icon_theme_treeview")), string_list, current_icon_theme, ICON_THEME_DEFAULT_NAME);
  g_list_free (string_list);
  g_list_free (theme_list);

  /* Finally, we do the Meta themes */
  theme_list = gnome_theme_meta_info_find_all ();
  string_list = NULL;
  for (list = theme_list; list; list = list->next)
    {
      GnomeThemeMetaInfo *info = list->data;

      if (! strcmp (info->gtk_theme_name, current_gtk_theme) &&
	  ! strcmp (info->icon_theme_name, current_icon_theme) &&
	  ! strcmp (info->metacity_theme_name, current_window_theme))
	{
	  current_meta_theme = g_strdup (info->name);
	}
      string_list = g_list_prepend (string_list, info->name);
    }

  if (current_meta_theme == NULL)
    current_meta_theme = g_strdup (_("Current modified"));

  load_theme_names (GTK_TREE_VIEW (WID ("meta_theme_treeview")), string_list, current_meta_theme, META_THEME_DEFAULT_NAME);
  g_list_free (string_list);
  g_list_free (theme_list);
  
  
  g_free (current_gtk_theme);
  g_free (current_icon_theme);
  g_free (current_meta_theme);
}


static void
theme_key_changed (GConfClient *client,
		   guint        cnxn_id,
		   GConfEntry  *entry,
		   gpointer     user_data)
{
  if (!strcmp (entry->key, GTK_THEME_KEY) || !strcmp (entry->key, ICON_THEME_KEY))
    {
      read_themes ((GladeXML *)user_data);
    }
}

static void
window_settings_changed (GnomeWindowManager *window_manager,
			 GladeXML           *dialog)
{
  /* We regretfully reread the entire capplet when this happens.
   * We should prolly change the lib to pass in a mask of changes.
   */
  read_themes (dialog);
}

static void
icon_key_changed (GConfClient *client,
		      guint        cnxn_id,
		      GConfEntry  *entry,
		      gpointer     user_data)
{
  if (strcmp (entry->key, ICON_THEME_KEY))
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
  gboolean a_default = FALSE;
  gboolean b_default = FALSE;
  gint retval;

  gtk_tree_model_get (model, a,
		      THEME_NAME_COLUMN, &a_str,
		      DEFAULT_THEME_COLUMN, &a_default,
		      -1);
  gtk_tree_model_get (model, b,
		      THEME_NAME_COLUMN, &b_str,
		      DEFAULT_THEME_COLUMN, &b_default,
		      -1);

  if (a_str == NULL) a_str = g_strdup ("");
  if (b_str == NULL) b_str = g_strdup ("");

  if (a_default)
    retval = -1;
  else if (b_default)
    retval = 1;
  else
    retval = g_utf8_collate (a_str, b_str);

  g_free (a_str);
  g_free (b_str);

  return retval;
}

/* Show the nautilus themes window */
static void
window_show_manage_themes (GtkWidget *button, gpointer data)
{
	gchar *path, *command;
	GnomeVFSURI *uri;
	GnomeWindowManager *wm;
	
	wm = gnome_wm_manager_get_current (gdk_display_get_default_screen (gdk_display_get_default ()));
	
	path = gnome_window_manager_get_user_theme_folder (wm);
	g_object_unref (G_OBJECT (wm));

	uri = gnome_vfs_uri_new (path);

	if (!gnome_vfs_uri_exists (uri)) {
		/* Create the directory */
		gnome_vfs_make_directory_for_uri (uri, 0775);
	}
	gnome_vfs_uri_unref (uri);


	command = g_strdup_printf ("nautilus --no-desktop %s", path);
	g_free (path);

	g_spawn_command_line_async (command, NULL);
	g_free (command);
}


static void
show_install_dialog (GtkWidget *button, gpointer parent)
{
  gnome_theme_installer_run (parent, NULL);
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
	gchar *filename = NULL;

	if (!(info == TARGET_URI_LIST || info == TARGET_NS_URL))
		return;

	uris = gnome_vfs_uri_list_parse ((gchar *) selection_data->data);
	if (uris != NULL && uris->data != NULL) {
		GnomeVFSURI *uri = (GnomeVFSURI *) uris->data;
		filename = gnome_vfs_unescape_string (
			gnome_vfs_uri_get_path (uri), G_DIR_SEPARATOR_S);
		gnome_vfs_uri_list_unref (uris);
	}

	gnome_theme_installer_run (widget, filename);
	g_free (filename);
}

/* Starts nautilus on the themes directory*/
static void
show_manage_themes (GtkWidget *button, gpointer data)
{
	gchar *path, *command;
	GnomeVFSURI *uri;

	path = g_strdup_printf ("%s/.themes", g_get_home_dir ());
	uri = gnome_vfs_uri_new (path);

	if (!gnome_vfs_uri_exists (uri)) {
		/* Create the directory */
		gnome_vfs_make_directory_for_uri (uri, 0775);
	}
	gnome_vfs_uri_unref (uri);

	command = g_strdup_printf ("nautilus --no-desktop %s", path);
	g_free (path);

	g_spawn_command_line_async (command, NULL);
	g_free (command);
}

static void
cb_dialog_response (GtkDialog *dialog, gint response_id)
{
	if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (dialog),
			"wgoscustdesk.xml",
			"goscustdesk-12");
	else
		gtk_main_quit ();
}

static void
setup_tree_view (GtkTreeView *tree_view,
		 GCallback    changed_callback,
		 GladeXML    *dialog)
{
  GtkTreeModel *model;
  GtkTreeSelection *selection;
 
  gtk_tree_view_insert_column_with_attributes (tree_view,
 					       -1, NULL,
 					       gtk_cell_renderer_text_new (),
 					       "text", THEME_NAME_COLUMN,
 					       NULL);
  
  model = (GtkTreeModel *) gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (model), 0, sort_func, NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model), 0, GTK_SORT_ASCENDING);
  gtk_tree_view_set_model (tree_view, model);
  selection = gtk_tree_view_get_selection (tree_view);
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
  g_signal_connect (G_OBJECT (selection), "changed", changed_callback, dialog);
}

static void
setup_dialog (GladeXML *dialog)
{
  GConfClient *client;
  GtkWidget *parent, *widget;
  GnomeWindowManager *window_manager;

  client = gconf_client_get_default ();
  window_manager = gnome_wm_manager_get_current (gdk_display_get_default_screen (gdk_display_get_default ()));
  parent = WID ("theme_dialog");

  setup_tree_view (GTK_TREE_VIEW (WID ("meta_theme_treeview")),
  		   (GCallback) meta_theme_selection_changed,
		   dialog);
  setup_tree_view (GTK_TREE_VIEW (WID ("control_theme_treeview")),
  		   (GCallback) gtk_theme_selection_changed,
		   dialog);
  setup_tree_view (GTK_TREE_VIEW (WID ("window_theme_treeview")),
  		   (GCallback) window_theme_selection_changed,
		   dialog);
  setup_tree_view (GTK_TREE_VIEW (WID ("icon_theme_treeview")),
  		   (GCallback) icon_theme_selection_changed,
		   dialog);

  gconf_client_add_dir (client, "/desktop/gnome/interface", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

  gconf_client_notify_add (client,
			   GTK_THEME_KEY,
			   (GConfClientNotifyFunc) &theme_key_changed,
			   dialog, NULL, NULL);
  gconf_client_notify_add (client,
			   ICON_THEME_KEY,
			   (GConfClientNotifyFunc) &icon_key_changed,
			   dialog, NULL, NULL);

  g_signal_connect (G_OBJECT (window_manager), "settings_changed", (GCallback)window_settings_changed, dialog);
  read_themes (dialog);

  gnome_theme_info_register_theme_change (theme_changed_func, dialog);

  /* gtk themes */
  widget = WID ("control_install_button");
  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (show_install_dialog), parent);
  widget = WID ("control_manage_button");
  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (show_manage_themes), dialog);

  /* window manager themes */
  widget = WID ("window_install_button");
  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (show_install_dialog), parent);
  widget = WID ("window_manage_button");
  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (window_show_manage_themes), dialog);

  /* icon themes */
  widget = WID ("icon_install_button");
  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (show_install_dialog), parent);
  widget = WID ("icon_manage_button");
  g_signal_connect (G_OBJECT (widget), "clicked",
		    G_CALLBACK (show_manage_themes), dialog);

  /*
  g_signal_connect (G_OBJECT (WID ("install_dialog")), "response",
		    G_CALLBACK (install_dialog_response), dialog);
  */

  g_signal_connect (G_OBJECT (parent),
		    "response",
		    G_CALLBACK (cb_dialog_response), NULL);

  gtk_drag_dest_set (parent, GTK_DEST_DEFAULT_ALL,
		     drop_types, n_drop_types,
		     GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_MOVE);

  g_signal_connect (G_OBJECT (parent), "drag-motion",
		    G_CALLBACK (drag_motion_cb), NULL);
  g_signal_connect (G_OBJECT (parent), "drag-leave",
		    G_CALLBACK (drag_leave_cb), NULL);
  g_signal_connect (G_OBJECT (parent), "drag-data-received",
		    G_CALLBACK (drag_data_received_cb),
		    dialog);

  capplet_set_icon (parent, "gnome-ccthemes.png");
  gtk_widget_show (parent);
}

int
main (int argc, char *argv[])
{
  GladeXML *dialog;

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);
  gnome_program_init ("gnome-theme-properties", VERSION,
		      LIBGNOMEUI_MODULE, argc, argv,
		      GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
		      NULL);

  gnome_wm_manager_init ();
  activate_settings_daemon ();
  dialog = create_dialog ();

  setup_dialog (dialog);
  gtk_main ();

  return 0;
}
