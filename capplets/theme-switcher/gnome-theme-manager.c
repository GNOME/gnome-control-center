/* This program was written under the GPL by Jonathan Blandford <jrb@gnome.org>
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
#include "gnome-theme-save.h"
#include "capplet-util.h"
#include "activate-settings-daemon.h"
#include "gconf-property-editor.h"
#include "file-transfer-dialog.h"
#include "gnome-theme-manager.h"
#include "gnome-theme-details.h"
#include "gnome-theme-installer.h"
#include "theme-thumbnail.h"


#define MAX_ELEMENTS_BEFORE_SCROLLING 3

/* Events: There are two types of change events we worry about.  The first is
 * when the theme settings change.  In this case, we can quickly update the UI
 * to reflect.  The other is when the themes themselves change.
 *
 * The code in gnome-theme-manager.c will update the main dialog and proxy the
 * update notifications for the details dialog.
 */

enum
{
  META_THEME_NAME_COLUMN = THEME_NAME_COLUMN,
  META_THEME_ID_COLUMN = THEME_ID_COLUMN,
  META_THEME_FLAG_COLUMN = THEME_FLAG_COLUMN,
  META_THEME_PIXBUF_COLUMN,
  META_N_COLUMNS
};

GtkTargetEntry drop_types[] =
{
  {"text/uri-list", 0, TARGET_URI_LIST},
  {"_NETSCAPE_URL", 0, TARGET_NS_URL}
};

gint n_drop_types = sizeof (drop_types) / sizeof (GtkTargetEntry);

static gboolean setting_model = FALSE;
static gboolean idle_running = FALSE;
static GdkPixbuf *default_image = NULL;

static GnomeThemeMetaInfo custom_meta_theme_info = {};

/* Function Prototypes */
static void      idle_async_func                 (GdkPixbuf          *pixbuf,
						  gpointer            data);
static void      list_data_free                  (gpointer            data);
static gboolean  load_theme_in_idle              (gpointer            data);
static void      add_pixbuf_idle                 (GtkTreeModel       *model);
static void      load_meta_themes                (GtkTreeView        *tree_view,
						  GList              *meta_theme_list,
						  char               *default_theme);
static void      meta_theme_setup_info           (GnomeThemeMetaInfo *meta_theme_info,
						  GladeXML           *dialog);
static void      meta_theme_set                  (GnomeThemeMetaInfo *meta_theme_info);
static void      meta_theme_selection_changed    (GtkTreeSelection   *selection,
						  GladeXML           *dialog);
static void      update_themes_from_disk         (GladeXML           *dialog);
static void      update_settings_from_gconf      (void);
static void      gtk_theme_key_changed           (GConfClient        *client,
						  guint               cnxn_id,
						  GConfEntry         *entry,
						  gpointer            user_data);
static void      window_settings_changed         (GnomeWindowManager *window_manager,
						  GladeXML           *dialog);
static void      icon_key_changed                (GConfClient        *client,
						  guint               cnxn_id,
						  GConfEntry         *entry,
						  gpointer            user_data);
static void      theme_changed_func              (gpointer            uri,
						  gpointer            user_data);
static void      cb_dialog_response              (GtkDialog          *dialog,
						  gint                response_id);
static void      setup_meta_tree_view            (GtkTreeView        *tree_view,
						  GCallback           changed_callback,
						  GladeXML           *dialog);
static void      setup_dialog                    (GladeXML           *dialog);


static void
idle_async_func (GdkPixbuf *pixbuf,
		 gpointer   data)
{
  GList *list = data;
  gchar *theme_id;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean valid;

  theme_id = list->next->data;
  model = list->data;

  for (valid = gtk_tree_model_get_iter_first (model, &iter);
       valid;
       valid = gtk_tree_model_iter_next (model, &iter))
    {
      gchar *test_theme_id;
      gtk_tree_model_get (model, &iter,
			  META_THEME_ID_COLUMN, &test_theme_id,
			  -1);
      if (test_theme_id && !strcmp (theme_id, test_theme_id))
	{
	  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			      META_THEME_PIXBUF_COLUMN, pixbuf,
			      -1);
	  g_free (test_theme_id);
	  break;
	}
      g_free (test_theme_id);
    }

  idle_running = FALSE;
  add_pixbuf_idle (model);
}

static void
list_data_free (gpointer data)
{
  GList *list_data = data;

  g_object_unref (G_OBJECT (list_data->data));
  g_free (list_data->next->data);
  g_list_free (list_data);
}


static gboolean
load_theme_in_idle (gpointer data)
{
  GtkTreeModel *model = data;
  GtkTreeIter iter;
  gboolean valid;

  for (valid = gtk_tree_model_get_iter_first (model, &iter);
       valid;
       valid = gtk_tree_model_iter_next (model, &iter))
    {
      GdkPixbuf *pixbuf = NULL;
      gchar *theme_id = NULL;

      gtk_tree_model_get (model, &iter,
			  META_THEME_PIXBUF_COLUMN, &pixbuf,
			  -1);
      if (pixbuf == default_image)
	{
	  GList *list_data = NULL;
	  GnomeThemeMetaInfo *meta_theme_info;

	  gtk_tree_model_get (model, &iter,
			      META_THEME_ID_COLUMN, &theme_id,
			      -1);
	  if (theme_id == NULL)
	    {
	      meta_theme_info = &custom_meta_theme_info;
	      continue;
	    }
	  else
	    {
	      meta_theme_info = gnome_theme_meta_info_find (theme_id);
	    }

	  /* We should always have a metatheme file */
	  g_assert (meta_theme_info);

	  list_data = g_list_prepend (list_data, theme_id);
	  g_object_ref (model);
	  list_data = g_list_prepend (list_data, model);
	  generate_theme_thumbnail_async (meta_theme_info,
					  idle_async_func,
					  list_data,
					  list_data_free);

	  return FALSE;
	}
    }

  /* If we're done loading all the main themes, lets initialize the details
   * dialog if it hasn't been done yet.  If it has, then this call is harmless.
   */
  gnome_theme_details_init ();

  return FALSE;
}

/* FIXME: we need a way to cancel the pixbuf loading if we get a theme updating
 * during the pixbuf generation.
 */
static void
add_pixbuf_idle (GtkTreeModel *model)
{
  if (idle_running)
    return;

  idle_running = TRUE;
  g_object_ref (model);
  g_idle_add_full (G_PRIORITY_LOW,
		   load_theme_in_idle,
		   model,
		   g_object_unref);
}

/* Loads up a list of GnomeMetaThemeInfo.
 */
static void
load_meta_themes (GtkTreeView *tree_view,
		  GList       *meta_theme_list,
		  char        *default_theme)
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

  for (list = meta_theme_list; list; list = list->next)
    {
      GnomeThemeMetaInfo *meta_theme_info = list->data;
      gchar *blurb;
      GtkTreeIter iter;
      gboolean is_default;
      GdkPixbuf *pixbuf;
      gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);

      if (strcmp (default_theme, meta_theme_info->name) == 0)
	is_default = TRUE;
      else
	is_default = FALSE;

      blurb = g_strdup_printf ("<span size=\"larger\" weight=\"bold\">%s</span>\n%s",
			       meta_theme_info->readable_name, meta_theme_info->comment);
      if (i <= MAX_ELEMENTS_BEFORE_SCROLLING)
	pixbuf = generate_theme_thumbnail (meta_theme_info);
      else
	pixbuf = default_image;

      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			  META_THEME_PIXBUF_COLUMN, pixbuf,
			  META_THEME_NAME_COLUMN, blurb,
			  META_THEME_ID_COLUMN, meta_theme_info->name,
			  META_THEME_FLAG_COLUMN, is_default ? THEME_FLAG_DEFAULT : 0,
			  -1);
      g_free (blurb);

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

  add_pixbuf_idle (model);
  setting_model = FALSE;
}

static void
meta_theme_setup_info (GnomeThemeMetaInfo *meta_theme_info,
		       GladeXML           *dialog)
{
  GtkWidget *notebook;

  notebook = WID ("meta_theme_notebook");

  if (meta_theme_info == NULL)
    {
      gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 0);
    }
  else
    {
      if (meta_theme_info->application_font != NULL)
	{
	  if (meta_theme_info->background_image != NULL)
	    gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 3);
	  else
	    gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 1);
	}
      else
	{
	  if (meta_theme_info->background_image != NULL)
	    gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 2);
	  else
	    gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 0);
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
			  META_THEME_ID_COLUMN, &meta_theme_name,
			  -1);
    }
  else
    {
      /* I probably just added a row. */
      return;
    }

  if (meta_theme_name)
    {
      meta_theme_info = gnome_theme_meta_info_find (meta_theme_name);
    }
  else
    {
      meta_theme_info = &custom_meta_theme_info;
    }
  meta_theme_setup_info (meta_theme_info, dialog);

  if (setting_model)
    return;

  if (meta_theme_info)
    meta_theme_set (meta_theme_info);
}

/* This function will adjust the list to reflect the current theme
 * situation.  It is called after the themes change on disk.  Currently, it
 * recreates the entire list.
 */
static void
update_themes_from_disk (GladeXML *dialog)
{
  GList *theme_list;
  GList *string_list;
  GList *list;
  GConfClient *client;
  gchar *current_gtk_theme;
  gchar *current_window_theme;
  gchar *current_icon_theme;
  GnomeWindowManager *window_manager;
  GnomeWMSettings wm_settings;
  GtkWidget *notebook;

  gboolean have_meta_theme;

  client = gconf_client_get_default ();

  current_gtk_theme = gconf_client_get_string (client, GTK_THEME_KEY, NULL);
  current_icon_theme = gconf_client_get_string (client, ICON_THEME_KEY, NULL);
  window_manager = gnome_wm_manager_get_current (gdk_display_get_default_screen (gdk_display_get_default ()));
  wm_settings.flags = GNOME_WM_SETTING_THEME;
  gnome_window_manager_get_settings (window_manager, &wm_settings);
  current_window_theme = g_strdup (wm_settings.theme);

  /* FIXME: What do we really do when there is no theme?  Ask Havoc here. */
  /* BROKEN BROKEN BROKEN */
  if (current_icon_theme == NULL)
    current_icon_theme = g_strdup ("Default");
  if (current_gtk_theme == NULL)
    current_gtk_theme = g_strdup ("Default");

  notebook = WID ("theme_notebook");

  theme_list = gnome_theme_meta_info_find_all ();
  string_list = NULL;
  for (list = theme_list; list; list = list->next)
    {
      GnomeThemeMetaInfo *info = list->data;

      string_list = g_list_prepend (string_list, info);
    }

  if (string_list == NULL)
    {
      have_meta_theme = FALSE;
    }
  else
    {
      have_meta_theme = TRUE;
      gtk_widget_show (WID ("meta_theme_hbox"));
      load_meta_themes (GTK_TREE_VIEW (WID ("meta_theme_treeview")), string_list, META_THEME_DEFAULT_NAME);
      g_list_free (string_list);
    }
  g_list_free (theme_list);

  g_free (current_gtk_theme);
  g_free (current_icon_theme);

  update_settings_from_gconf ();

  if (! have_meta_theme)
    {
      GtkWidget *dialog;

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

static void
add_custom_row_to_meta_theme (const gchar  *current_gtk_theme,
			      const gchar  *current_window_theme,
			      const gchar  *current_icon_theme)
{
  GladeXML *dialog;
  GtkWidget *tree_view;
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  gboolean valid;
  gchar *blurb;

  dialog = gnome_theme_manager_get_theme_dialog ();
  tree_view = WID ("meta_theme_treeview");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));
  
  custom_meta_theme_info.gtk_theme_name = g_strdup (current_gtk_theme);
  custom_meta_theme_info.metacity_theme_name = g_strdup (current_window_theme);
  custom_meta_theme_info.icon_theme_name = g_strdup (current_icon_theme);

  for (valid = gtk_tree_model_get_iter_first (model, &iter);
       valid;
       valid = gtk_tree_model_iter_next (model, &iter))
    {
      guint theme_flags = 0;

      gtk_tree_model_get (model, &iter,
			  META_THEME_FLAG_COLUMN, &theme_flags,
			  -1);
      if (theme_flags & THEME_FLAG_CUSTOM)
	break;

    }

  /* if we found a custom row and broke out of the list above, valid will be
   * TRUE.  If we didn't, we need to add a new iter.
   */
  if (!valid)
    gtk_list_store_prepend (GTK_LIST_STORE (model), &iter);

  /* set the values of the Custom theme. */
  blurb = g_strdup_printf ("<span size=\"larger\" weight=\"bold\">%s</span>\n%s",
			   _("Custom theme"), _("You can save this theme by pressing the Save Theme button."));

  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		      META_THEME_NAME_COLUMN, blurb,
		      META_THEME_FLAG_COLUMN, THEME_FLAG_CUSTOM,
		      META_THEME_PIXBUF_COLUMN, default_image,
		      -1);
  gtk_widget_set_sensitive (WID ("meta_theme_save_button"), TRUE);
  path = gtk_tree_model_get_path (model, &iter);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree_view), path, NULL, FALSE);
  gtk_tree_path_free (path);
  g_free (blurb);
}


static void
remove_custom_row_from_meta_theme (GtkTreeModel *model)
{
  GladeXML *dialog;
  GtkTreeIter iter;
  GtkTreeIter next_iter;
  gboolean valid;

  dialog = gnome_theme_manager_get_theme_dialog ();

  valid = gtk_tree_model_get_iter_first (model, &iter);
  while (valid)
    {
      guint theme_flags = 0;

      next_iter = iter;
      valid = gtk_tree_model_iter_next (model, &next_iter);

      gtk_tree_model_get (model, &iter,
			  META_THEME_FLAG_COLUMN, &theme_flags,
			  -1);

      if (theme_flags & THEME_FLAG_CUSTOM)
	{
	  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
	}
      iter = next_iter;
    }
  g_free (custom_meta_theme_info.gtk_theme_name);
  g_free (custom_meta_theme_info.metacity_theme_name);
  g_free (custom_meta_theme_info.icon_theme_name);

  gtk_widget_set_sensitive (WID ("meta_theme_save_button"), FALSE);

  custom_meta_theme_info.gtk_theme_name = NULL;
  custom_meta_theme_info.metacity_theme_name = NULL;
  custom_meta_theme_info.icon_theme_name = NULL;
}


/* Sets the list to point to the current theme.  Also creates the 'Custom Theme'
 * field if needed */
static void
update_settings_from_gconf (void)
{
  GConfClient *client;
  gchar *current_gtk_theme;
  gchar *current_window_theme;
  gchar *current_icon_theme;
  GnomeWindowManager *window_manager;
  GnomeWMSettings wm_settings;
  GtkWidget *tree_view;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GladeXML *dialog;
  gboolean valid;
  gboolean custom_theme_found;

  client = gconf_client_get_default ();

  /* Get the settings */
  current_gtk_theme = gconf_client_get_string (client, GTK_THEME_KEY, NULL);
  current_icon_theme = gconf_client_get_string (client, ICON_THEME_KEY, NULL);
  window_manager = gnome_wm_manager_get_current (gdk_display_get_default_screen (gdk_display_get_default ()));
  wm_settings.flags = GNOME_WM_SETTING_THEME;
  gnome_window_manager_get_settings (window_manager, &wm_settings);
  current_window_theme = g_strdup (wm_settings.theme);
  custom_theme_found = TRUE;

  /* Walk the tree looking for the current one. */
  dialog = gnome_theme_manager_get_theme_dialog ();
  tree_view = WID ("meta_theme_treeview");
  g_assert (tree_view);
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));

  for (valid = gtk_tree_model_get_iter_first (model, &iter);
       valid;
       valid = gtk_tree_model_iter_next (model, &iter))
    {
      gchar *row_theme_id = NULL;
      guint row_theme_flags = 0;
      GnomeThemeMetaInfo *meta_theme_info;

      gtk_tree_model_get (model, &iter,
			  META_THEME_ID_COLUMN, &row_theme_id,
			  META_THEME_FLAG_COLUMN, &row_theme_flags,
			  -1);

      if (row_theme_flags & THEME_FLAG_CUSTOM)
	{
	  meta_theme_info = &custom_meta_theme_info;
	}
      else if (row_theme_id)
	{
	  meta_theme_info = gnome_theme_meta_info_find (row_theme_id);
	}
      else
	{
	  meta_theme_info = &custom_meta_theme_info;
	}
      g_free (row_theme_id);

      if (meta_theme_info == &(custom_meta_theme_info))
	custom_theme_found = TRUE;
      else if (! strcmp (current_gtk_theme, meta_theme_info->gtk_theme_name) &&
	  ! strcmp (current_window_theme, meta_theme_info->metacity_theme_name) &&
	  ! strcmp (current_icon_theme, meta_theme_info->icon_theme_name))
	{
	  GtkTreePath *path;

	  path = gtk_tree_model_get_path (model, &iter);
	  gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree_view), path, NULL, FALSE);
	  gtk_tree_path_free (path);
	  custom_theme_found = FALSE;

	  break;
	}
    }

  if (custom_theme_found)
    add_custom_row_to_meta_theme (current_gtk_theme, current_window_theme, current_icon_theme);
  else
    remove_custom_row_from_meta_theme (model);

  g_free (current_gtk_theme);
  g_free (current_window_theme);
  g_free (current_icon_theme);
  add_pixbuf_idle (model);
}

static void
gtk_theme_key_changed (GConfClient *client,
		       guint        cnxn_id,
		       GConfEntry  *entry,
		       gpointer     user_data)
{
  if (strcmp (entry->key, GTK_THEME_KEY))
    return;

  update_settings_from_gconf ();
  gnome_theme_details_update_from_gconf ();
}

static void
window_settings_changed (GnomeWindowManager *window_manager,
			 GladeXML           *dialog)
{
  /* There is no way to know what changed, so we just check it all */
  update_settings_from_gconf ();
  gnome_theme_details_update_from_gconf ();
}

static void
icon_key_changed (GConfClient *client,
		  guint        cnxn_id,
		  GConfEntry  *entry,
		  gpointer     user_data)
{
  if (strcmp (entry->key, ICON_THEME_KEY))
    return;

  update_settings_from_gconf ();
  gnome_theme_details_update_from_gconf ();
}

/* FIXME: We want a more sophisticated theme_changed func sometime */
static void
theme_changed_func (gpointer uri,
		    gpointer user_data)
{
  GladeXML *dialog;

  dialog = gnome_theme_manager_get_theme_dialog ();

  g_print ("theme_changed_func:\n");
  update_themes_from_disk ((GladeXML *)user_data);
  gnome_theme_details_reread_themes_from_disk ();
  gtk_widget_grab_focus (WID ("meta_theme_treeview"));
}

static void
cb_dialog_response (GtkDialog *dialog, gint response_id)
{
  if (response_id == GTK_RESPONSE_HELP)
    capplet_help (GTK_WINDOW (dialog), "wgoscustdesk.xml", "goscustdesk-12");
  else
    gtk_main_quit ();
}

static void
setup_meta_tree_view (GtkTreeView *tree_view,
		      GCallback    changed_callback,
		      GladeXML    *dialog)
{
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  GtkCellRenderer *renderer;

  renderer = g_object_new (GTK_TYPE_CELL_RENDERER_PIXBUF,
			   "xpad", 4,
			   "ypad", 4,
			   NULL);

  gtk_tree_view_insert_column_with_attributes (tree_view,
 					       -1, NULL,
 					       renderer,
 					       "pixbuf", META_THEME_PIXBUF_COLUMN,
 					       NULL);
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_insert_column_with_attributes (tree_view,
 					       -1, NULL,
 					       renderer,
 					       "markup", META_THEME_NAME_COLUMN,
 					       NULL);

  model = (GtkTreeModel *) gtk_list_store_new (META_N_COLUMNS,
					       G_TYPE_STRING,    /* META_THEME_NAME_COLUMN   */
					       G_TYPE_STRING,    /* META_THEME_ID_COLUMN     */
					       G_TYPE_UINT,      /* META_THEME_FLAG_COLUMN   */
					       GDK_TYPE_PIXBUF); /* META_THEME_PIXBUF_COLUMN */
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (model), 0, gnome_theme_manager_tree_sort_func, NULL, NULL);
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (model), 0, GTK_SORT_ASCENDING);
  gtk_tree_view_set_model (tree_view, model);
  selection = gtk_tree_view_get_selection (tree_view);
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
  g_signal_connect (G_OBJECT (selection), "changed", changed_callback, dialog);
}

static void
gnome_theme_save_clicked (GtkWidget *button,
			  gpointer   data)
{
  GladeXML *dialog;

  dialog = gnome_theme_manager_get_theme_dialog ();

  gnome_theme_save_show_dialog (WID ("theme_dialog"), &custom_meta_theme_info);
}

static void
setup_dialog (GladeXML *dialog)
{
  GConfClient *client;
  GtkWidget *parent, *widget;
  GnomeWindowManager *window_manager;
  GtkSizeGroup *size_group;

  client = gconf_client_get_default ();
  window_manager = gnome_wm_manager_get_current (gdk_display_get_default_screen (gdk_display_get_default ()));
  parent = WID ("theme_dialog");

  size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  gtk_size_group_add_widget (size_group, WID ("meta_theme_details_button"));
  gtk_size_group_add_widget (size_group, WID ("meta_theme_install_button"));
  gtk_size_group_add_widget (size_group, WID ("meta_theme_save_button"));
  gtk_size_group_add_widget (size_group, WID ("meta_theme_font1_button"));
  gtk_size_group_add_widget (size_group, WID ("meta_theme_background1_button"));
  gtk_size_group_add_widget (size_group, WID ("meta_theme_font2_button"));
  gtk_size_group_add_widget (size_group, WID ("meta_theme_background2_button"));
  g_object_unref (size_group);

  g_signal_connect (G_OBJECT (WID ("meta_theme_details_button")), "clicked", gnome_theme_details_show, NULL);


  setup_meta_tree_view (GTK_TREE_VIEW (WID ("meta_theme_treeview")),
			(GCallback) meta_theme_selection_changed,
			dialog);

  gconf_client_add_dir (client, "/desktop/gnome/interface", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

  gconf_client_notify_add (client,
			   GTK_THEME_KEY,
			   (GConfClientNotifyFunc) &gtk_theme_key_changed,
			   dialog, NULL, NULL);
  gconf_client_notify_add (client,
			   ICON_THEME_KEY,
			   (GConfClientNotifyFunc) &icon_key_changed,
			   dialog, NULL, NULL);

  g_signal_connect (G_OBJECT (window_manager), "settings_changed", (GCallback) window_settings_changed, dialog);
  update_themes_from_disk (dialog);
  gtk_widget_grab_focus (WID ("meta_theme_treeview"));
  gnome_theme_info_register_theme_change (theme_changed_func, dialog);

  /* gtk themes */
  widget = WID ("control_install_button");
  g_signal_connect_swapped (G_OBJECT (widget), "clicked", G_CALLBACK (gnome_theme_installer_run), parent);
  widget = WID ("control_manage_button");
  g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (gnome_theme_manager_show_manage_themes), dialog);

  /* window manager themes */
  widget = WID ("window_install_button");
  g_signal_connect_swapped (G_OBJECT (widget), "clicked", G_CALLBACK (gnome_theme_installer_run), parent);
  widget = WID ("window_manage_button");
  g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (gnome_theme_manager_window_show_manage_themes), dialog);

  /* icon themes */
  widget = WID ("icon_install_button");
  g_signal_connect_swapped (G_OBJECT (widget), "clicked", G_CALLBACK (gnome_theme_installer_run), parent);
  widget = WID ("icon_manage_button");
  g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (gnome_theme_manager_show_manage_themes), dialog);

  widget = WID ("meta_theme_save_button");
  g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (gnome_theme_save_clicked), NULL);


/*
  g_signal_connect (G_OBJECT (WID ("install_dialog")), "response",
		    G_CALLBACK (install_dialog_response), dialog);
  */

  g_signal_connect (G_OBJECT (parent), "response", G_CALLBACK (cb_dialog_response), NULL);

  gtk_drag_dest_set (parent, GTK_DEST_DEFAULT_ALL,
		     drop_types, n_drop_types,
		     GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_MOVE);
  g_signal_connect (G_OBJECT (parent), "drag-motion", G_CALLBACK (gnome_theme_manager_drag_motion_cb), NULL);
  g_signal_connect (G_OBJECT (parent), "drag-leave", G_CALLBACK (gnome_theme_manager_drag_leave_cb), NULL);
  g_signal_connect (G_OBJECT (parent), "drag-data-received",G_CALLBACK (gnome_theme_manager_drag_data_received_cb), NULL);

  capplet_set_icon (parent, "gnome-ccthemes.png");
  gtk_widget_show (parent);
}

/* Non static functions */
GladeXML *
gnome_theme_manager_get_theme_dialog (void)
{
  static GladeXML *dialog = NULL;

  if (dialog == NULL)
    dialog = glade_xml_new (GLADEDIR "/theme-properties.glade", NULL, NULL);

  return dialog;
}

gint
gnome_theme_manager_tree_sort_func (GtkTreeModel *model,
				    GtkTreeIter  *a,
				    GtkTreeIter  *b,
				    gpointer      user_data)
{
  gchar *a_str = NULL;
  gchar *b_str = NULL;
  guint a_default = FALSE;
  guint b_default = FALSE;
  gint retval;

  gtk_tree_model_get (model, a,
		      META_THEME_NAME_COLUMN, &a_str,
		      META_THEME_FLAG_COLUMN, &a_default,
		      -1);
  gtk_tree_model_get (model, b,
		      META_THEME_NAME_COLUMN, &b_str,
		      META_THEME_FLAG_COLUMN, &b_default,
		      -1);

  if (a_str == NULL) a_str = g_strdup ("");
  if (b_str == NULL) b_str = g_strdup ("");

  if (a_default & THEME_FLAG_CUSTOM)
    retval = -1;
  else if (b_default & THEME_FLAG_CUSTOM)
    retval = 1;
  else if (a_default & THEME_FLAG_DEFAULT)
    retval = -1;
  else if (b_default & THEME_FLAG_DEFAULT)
    retval = 1;
  else
    retval = g_utf8_collate (a_str, b_str);

  g_free (a_str);
  g_free (b_str);

  return retval;
}

/* Starts nautilus on the themes directory*/
void
gnome_theme_manager_show_manage_themes (GtkWidget *button, gpointer data)
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

/* Show the nautilus themes window */
void
gnome_theme_manager_window_show_manage_themes (GtkWidget *button, gpointer data)
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

/* Callback issued during drag movements */
gboolean
gnome_theme_manager_drag_motion_cb (GtkWidget *widget, GdkDragContext *context,
				    gint x, gint y, guint time, gpointer data)
{
	return FALSE;
}

/* Callback issued during drag leaves */
void
gnome_theme_manager_drag_leave_cb (GtkWidget *widget, GdkDragContext *context,
	       guint time, gpointer data)
{
	gtk_widget_queue_draw (widget);
}

/* Callback issued on actual drops. Attempts to load the file dropped. */
void
gnome_theme_manager_drag_data_received_cb (GtkWidget *widget, GdkDragContext *context,
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


int
main (int argc, char *argv[])
{
  GladeXML *dialog;

  /* We need to do this before we initialize anything else */
  theme_thumbnail_factory_init (argc, argv);

  bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  gnome_program_init ("gnome-theme-properties", VERSION,
		      LIBGNOMEUI_MODULE, argc, argv,
		      GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
		      NULL);

  gnome_wm_manager_init ();
  activate_settings_daemon ();

  dialog = gnome_theme_manager_get_theme_dialog ();
  setup_dialog (dialog);

  gtk_main ();

  return 0;
}
