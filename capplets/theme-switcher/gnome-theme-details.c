
#include <gtk/gtk.h>
#include <config.h>

#include <string.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>
#include <libgnomevfs/gnome-vfs-async-ops.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-utils.h>

#include <libwindow-settings/gnome-wm-manager.h>

#include "capplet-util.h"
#include "gnome-theme-manager.h"
#include "gnome-theme-details.h"
#include "gnome-theme-installer.h"
#include "gnome-theme-info.h"

#define MAX_ELEMENTS_BEFORE_SCROLLING 12

static gboolean theme_details_initted = FALSE;
static gboolean setting_model = FALSE;

/* Function Prototypes */
static void cb_dialog_response              (GtkDialog        *dialog,
					     gint              response_id);
static void setup_tree_view                 (GtkTreeView      *tree_view,
					     GCallback         changed_callback,
					     GladeXML         *dialog);
static void gtk_theme_selection_changed     (GtkTreeSelection *selection,
					     gpointer          data);
static void window_theme_selection_changed  (GtkTreeSelection *selection,
					     gpointer          data);
static void icon_theme_selection_changed    (GtkTreeSelection *selection,
					     gpointer          data);
static void update_gconf_key_from_selection (GtkTreeSelection *selection,
					     const gchar      *gconf_key);
static void load_theme_names                (GtkTreeView        *tree_view,
					     GList              *theme_list,
					     const gchar        *default_theme);
static char *path_to_theme_id               (const char *path);


static char *
path_to_theme_id (const char *path)
{
  char *dirname;
  char *result;

  dirname = g_path_get_dirname(path);
  result = g_path_get_basename(dirname);
  g_free(dirname);

  return result;
}

static void
cb_dialog_response (GtkDialog *dialog, gint response_id)
{
  if (response_id == GTK_RESPONSE_HELP)
    capplet_help (GTK_WINDOW (dialog), "wgoscustdesk.xml", "goscustdesk-12");
  else
    gtk_widget_hide (GTK_WIDGET (dialog));
}

static gint
details_tree_sort_func (GtkTreeModel *model,
			GtkTreeIter  *a,
			GtkTreeIter  *b,
			gpointer      user_data)
{
  gchar *a_name = NULL;
  gchar *b_name = NULL;
  guint a_flag = FALSE;
  guint b_flag = FALSE;
  gint retval;

  gtk_tree_model_get (model, a,
		      THEME_NAME_COLUMN, &a_name,
		      THEME_FLAG_COLUMN, &a_flag,
		      -1);
  gtk_tree_model_get (model, b,
		      THEME_NAME_COLUMN, &b_name,
		      THEME_FLAG_COLUMN, &b_flag,
		      -1);

  retval = gnome_theme_manager_sort_func (a_name, b_name, a_flag, b_flag);

  g_free (a_name);
  g_free (b_name);

  return retval;
}

static void
setup_tree_view (GtkTreeView *tree_view,
		 GCallback    changed_callback,
		 GladeXML    *dialog)
{
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  GtkCellRenderer *renderer;

  model = (GtkTreeModel *) gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (model), 0, details_tree_sort_func, NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model), 0, GTK_SORT_ASCENDING);
  gtk_tree_view_set_model (tree_view, model);

  selection = gtk_tree_view_get_selection (tree_view);
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
  g_signal_connect (G_OBJECT (selection), "changed", changed_callback, dialog);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (tree_view,
 					       -1, NULL,
 					       renderer,
 					       "text", THEME_NAME_COLUMN,
 					       NULL);
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
			  THEME_ID_COLUMN, &window_theme_name,
			  -1);
    }
  else
    {
      return;
    }

  window_manager = gnome_wm_manager_get_current (gdk_display_get_default_screen (gdk_display_get_default ()));
  if (window_manager != NULL) {
    wm_settings.flags = GNOME_WM_SETTING_THEME;
    wm_settings.theme = window_theme_name;
    gnome_window_manager_change_settings (window_manager, &wm_settings);
  }
  g_free (window_theme_name);
}

static void
gnome_theme_installer_run_cb (GtkWidget *button,
			      GtkWidget *parent_window)
{
  gnome_theme_installer_run (parent_window, NULL, FALSE);
}


static void
gnome_icon_theme_installer_run_cb (GtkWidget *button,
			      GtkWidget *parent_window)
{
  gnome_theme_installer_run (parent_window, NULL, TRUE);
}

static void
icon_theme_selection_changed (GtkTreeSelection *selection,
			     gpointer          data)
{
  update_gconf_key_from_selection (selection, ICON_THEME_KEY);
}

static void
load_theme_names (GtkTreeView *tree_view,
		  GList       *theme_list,
		  const gchar *default_theme)
{
  GList *list;
  GtkTreeModel *model;
  GtkWidget *swindow;
  gint i = 0;

  swindow = GTK_WIDGET (tree_view)->parent;
  model = gtk_tree_view_get_model (tree_view);
  g_assert (model);

  setting_model = TRUE;
  gtk_list_store_clear (GTK_LIST_STORE (model));

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
				  GTK_POLICY_NEVER, GTK_POLICY_NEVER);
  gtk_widget_set_usize (swindow, -1, -1);

  for (list = theme_list; list; list = list->next->next)
    {
      const char *name = list->data;
      const char *id = list->next->data;
      GtkTreeIter iter;
      gboolean is_default;

      gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);

      if (default_theme && strcmp (default_theme, name) == 0)
	is_default = TRUE;
      else
	is_default = FALSE;

      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			  THEME_NAME_COLUMN, name,
			  THEME_ID_COLUMN, id,
			  THEME_FLAG_COLUMN, is_default,
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
			  THEME_ID_COLUMN, &new_key,
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

void
gnome_theme_details_init (void)
{
  GConfClient *client;
  GtkWidget *parent, *widget;
  GnomeWindowManager *window_manager;
  GladeXML *dialog;

  if (theme_details_initted)
    return;
  theme_details_initted = TRUE;

  dialog = gnome_theme_manager_get_theme_dialog ();
  client = gconf_client_get_default ();
  window_manager = gnome_wm_manager_get_current (gdk_display_get_default_screen (gdk_display_get_default ()));
  parent = WID ("theme_details_dialog");

  setup_tree_view (GTK_TREE_VIEW (WID ("control_theme_treeview")),
  		   (GCallback) gtk_theme_selection_changed,
		   dialog);
  setup_tree_view (GTK_TREE_VIEW (WID ("window_theme_treeview")),
  		   (GCallback) window_theme_selection_changed,
		   dialog);
  setup_tree_view (GTK_TREE_VIEW (WID ("icon_theme_treeview")),
  		   (GCallback) icon_theme_selection_changed,
		   dialog);

  /* gtk themes */
  widget = WID ("control_install_button");
  g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (gnome_theme_installer_run_cb), parent);
  widget = WID ("control_manage_button");
  g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (gnome_theme_manager_show_manage_themes), dialog);

  /* window manager themes */
  widget = WID ("window_install_button");
  g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (gnome_theme_installer_run_cb), parent);
  widget = WID ("window_manage_button");
  g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (gnome_theme_manager_window_show_manage_themes), dialog);

  /* icon themes */
  widget = WID ("icon_install_button");
  g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (gnome_icon_theme_installer_run_cb), parent);
  widget = WID ("icon_manage_button");
  g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (gnome_theme_manager_icon_show_manage_themes), dialog);


  g_signal_connect (G_OBJECT (parent), "response", G_CALLBACK (cb_dialog_response), NULL);
  g_signal_connect (G_OBJECT (parent), "delete-event", G_CALLBACK (gtk_true), NULL);

  gtk_drag_dest_set (parent, GTK_DEST_DEFAULT_ALL,
		     drop_types, n_drop_types,
		     GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_MOVE);
  g_signal_connect (G_OBJECT (parent), "drag-motion", G_CALLBACK (gnome_theme_manager_drag_motion_cb), NULL);
  g_signal_connect (G_OBJECT (parent), "drag-leave", G_CALLBACK (gnome_theme_manager_drag_leave_cb), NULL);
  g_signal_connect (G_OBJECT (parent), "drag-data-received", G_CALLBACK (gnome_theme_manager_drag_data_received_cb), NULL);

  capplet_set_icon (parent, "gnome-ccthemes.png");

  gnome_theme_details_reread_themes_from_disk ();
}

void
gnome_theme_details_show (void)
{
  GladeXML *dialog;
  GtkWidget *parent;

  gnome_theme_details_init ();

  dialog = gnome_theme_manager_get_theme_dialog ();
  parent = WID ("theme_details_dialog");
  gtk_widget_show (parent);
  gtk_window_present (GTK_WINDOW (parent));
}

static void
warn_about_no_themes (void)
{
  static GtkWidget *dialog;

  if (dialog == NULL)
    {
      dialog = gtk_message_dialog_new (NULL,
				       GTK_DIALOG_MODAL,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_OK,
				       _("No themes could be found on your system.  This probably means that your \"Theme Preferences\" dialog was improperly installed, or you haven't installed the \"gnome-themes\" package."));
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      exit (0);
    }
}

void
gnome_theme_details_reread_themes_from_disk (void)
{
  GladeXML *dialog;
  GList *theme_list;
  GList *string_list;
  GList *list;
  GnomeWindowManager *window_manager;
  GtkWidget *notebook;

  gboolean have_gtk_theme;
  gboolean have_window_theme;
  gboolean have_icon_theme;

  
  gnome_theme_details_init ();

  dialog = gnome_theme_manager_get_theme_dialog ();
  window_manager = gnome_wm_manager_get_current (gdk_display_get_default_screen (gdk_display_get_default ()));
  notebook = WID ("theme_notebook");

  /* First, we update the GTK+ themes page */
  theme_list = gnome_theme_info_find_by_type (GNOME_THEME_GTK_2);
  string_list = NULL;
  for (list = theme_list; list; list = list->next)
    {
      GnomeThemeInfo *info = list->data;
      /* Use same string for ID as for name with GTK themes */
      string_list = g_list_prepend (string_list, info->name);
      string_list = g_list_prepend (string_list, info->name);
    }

  if (string_list == NULL)
    {
      have_gtk_theme = FALSE;
      gtk_widget_hide (WID ("control_theme_vbox"));
    }
  else
    {
      have_gtk_theme = TRUE;
      gtk_widget_show (WID ("control_theme_vbox"));
      load_theme_names (GTK_TREE_VIEW (WID ("control_theme_treeview")), string_list, gtk_theme_default_name);
      g_list_free (string_list);
    }
  g_list_free (theme_list);

  /* Next, we do the window managers */
  theme_list = window_manager ? gnome_window_manager_get_theme_list (window_manager) : NULL;
  string_list = NULL;
  for (list = theme_list; list; list = list->next)
    {
      /* Use same string for ID as for name with Window themes */
      string_list = g_list_prepend (string_list, list->data);
      string_list = g_list_prepend (string_list, list->data);
    }  
  if (string_list == NULL)
    {
      have_window_theme = FALSE;
      gtk_widget_hide (WID ("window_theme_vbox"));
    }
  else
    {
      have_window_theme = TRUE;
      gtk_widget_show (WID ("window_theme_vbox"));
      load_theme_names (GTK_TREE_VIEW (WID ("window_theme_treeview")), string_list, window_theme_default_name);
      g_list_free (string_list);
    }
  g_list_free (theme_list);

  /* Third, we do the icon theme */
  theme_list = gnome_theme_icon_info_find_all ();
  string_list = NULL;

  for (list = theme_list; list; list = list->next)
    {
      GnomeThemeIconInfo *info = list->data;
      string_list = g_list_prepend (string_list, path_to_theme_id(info->path));
      string_list = g_list_prepend (string_list, info->name);
    }

  if (string_list == NULL)
    {
      have_icon_theme = FALSE;
      gtk_widget_hide (WID ("icon_theme_vbox"));
    }
  else
    {
      have_icon_theme = TRUE;
      gtk_widget_show (WID ("icon_theme_vbox"));
      load_theme_names (GTK_TREE_VIEW (WID ("icon_theme_treeview")), string_list, icon_theme_default_name);
      g_list_free (string_list);
    }
  g_list_free (theme_list);

  if (! have_gtk_theme && ! have_window_theme && ! have_icon_theme)
    warn_about_no_themes ();

  gnome_theme_details_update_from_gconf ();

}

static void
update_list_something (GtkWidget *tree_view, const gchar *theme)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreeIter next_iter;
  gboolean valid;
  gboolean theme_found;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
  g_assert (model);

  valid = gtk_tree_model_get_iter_first (model, &iter);
  theme_found = FALSE;

  while (valid)
    {
      gchar *theme_id;
      guint flags = 0;

      next_iter = iter;
      valid = gtk_tree_model_iter_next (model, &next_iter);

      gtk_tree_model_get (model, &iter,
			  THEME_ID_COLUMN, &theme_id,
			  THEME_FLAG_COLUMN, &flags,
			  -1);

      if (! strcmp (theme, theme_id))
	{
	  GtkTreePath *path;
	  path = gtk_tree_model_get_path (model, &iter);
	  gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree_view), path, NULL, FALSE);
	  gtk_tree_path_free (path);
	  theme_found = TRUE;
	}
      else
	{
	  if (flags & THEME_FLAG_CUSTOM)
	    {
	      gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
	    }
	}
      g_free (theme_id);
      iter = next_iter;
      if (theme_found)
	break;
    }

  if (theme_found == FALSE)
    /* Never found the theme. */
    {
      GtkTreePath *path;

      gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			  THEME_NAME_COLUMN, theme,
			  THEME_ID_COLUMN, theme,
			  THEME_FLAG_COLUMN, THEME_FLAG_CUSTOM,
			  -1);
      path = gtk_tree_model_get_path (model, &iter);
      gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree_view), path, NULL, FALSE);
      gtk_tree_path_free (path);
    }
}

void
gnome_theme_details_update_from_gconf (void)
{
  GConfClient *client;
  GladeXML *dialog;
  GtkWidget *tree_view;
  gchar *theme;
  GnomeWindowManager *window_manager;
  GnomeWMSettings wm_settings;

  gnome_theme_details_init ();

  window_manager = gnome_wm_manager_get_current (gdk_display_get_default_screen (gdk_display_get_default ()));

  client = gconf_client_get_default ();
  dialog = gnome_theme_manager_get_theme_dialog ();

  /* FIXME: What do we really do when there is no theme?  Ask Havoc here. */
  tree_view = WID ("control_theme_treeview");
  theme = gconf_client_get_string (client, GTK_THEME_KEY, NULL);
  update_list_something (tree_view, theme);
  g_free (theme);

  tree_view = WID ("window_theme_treeview");
  wm_settings.flags = GNOME_WM_SETTING_THEME;
  if (window_manager) {
    gnome_window_manager_get_settings (window_manager, &wm_settings);
    update_list_something (tree_view, wm_settings.theme);
  }

  tree_view = WID ("icon_theme_treeview");
  theme = gconf_client_get_string (client, ICON_THEME_KEY, NULL);
  update_list_something (tree_view, theme);
  g_free (theme);
}
