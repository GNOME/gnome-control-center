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
static gboolean initial_meta_theme_set = FALSE;
static GdkPixbuf *default_image = NULL;

static GnomeThemeMetaInfo custom_meta_theme_info = {};
static GnomeThemeMetaInfo initial_meta_theme_info = {};

const char *meta_theme_default_name = NULL; 
const char *gtk_theme_default_name = NULL; 
const char *window_theme_default_name = NULL; 
const char *icon_theme_default_name = NULL; 


/* Function Prototypes */
static void      idle_async_func                 (GdkPixbuf          *pixbuf,
						  gpointer            data);
static void      list_data_free                  (gpointer            data);
static gboolean  load_theme_in_idle              (gpointer            data);
static void      add_pixbuf_idle                 (void);
static void      load_meta_themes                (GtkTreeView        *tree_view,
						  GList              *meta_theme_list);
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
static void      font_key_changed                (GConfClient        *client,
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
  add_pixbuf_idle ();
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
  GladeXML *dialog;
  GtkWidget *tree_view;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean valid;

  dialog = gnome_theme_manager_get_theme_dialog ();
  tree_view = WID ("meta_theme_treeview");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));

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
  idle_running = FALSE;

  return FALSE;
}

/* FIXME: we need a way to cancel the pixbuf loading if we get a theme updating
 * during the pixbuf generation.
 */
static void
add_pixbuf_idle (void)
{
  if (idle_running)
    return;

  idle_running = TRUE;
  g_idle_add_full (G_PRIORITY_LOW,
		   load_theme_in_idle,
		   NULL, NULL);
}

static gint
sort_meta_theme_list_func (gconstpointer  a,
			   gconstpointer  b)
{
  const GnomeThemeMetaInfo *a_meta_theme_info = a;
  const GnomeThemeMetaInfo *b_meta_theme_info = b;
  guint a_flag = 0;
  guint b_flag = 0;

  g_assert (a_meta_theme_info->name);
  g_assert (b_meta_theme_info->name);

  if (meta_theme_default_name && strcmp (meta_theme_default_name, a_meta_theme_info->name) == 0)
    a_flag |= THEME_FLAG_DEFAULT;
  if (meta_theme_default_name && strcmp (meta_theme_default_name, b_meta_theme_info->name) == 0)
    b_flag |= THEME_FLAG_DEFAULT;

  return gnome_theme_manager_sort_func (a_meta_theme_info->readable_name,
					b_meta_theme_info->readable_name,
					a_flag,
					b_flag);
}


/* Loads up a list of GnomeThemeMetaInfo.
 */
static void
load_meta_themes (GtkTreeView *tree_view,
		  GList       *meta_theme_list)
{
  GList *list;
  GtkTreeModel *model;
  GtkWidget *swindow;
  GtkTreeIter iter;
  gchar *name;
  gboolean valid;
  guint flag;
  gint i = 0;
  GConfClient *client;
  gchar *current_gtk_theme;
  gchar *current_window_theme;
  gchar *current_icon_theme;
  GnomeWindowManager *window_manager;
  GnomeWMSettings wm_settings;
  static gboolean first_time = TRUE;

  swindow = GTK_WIDGET (tree_view)->parent;
  model = gtk_tree_view_get_model (tree_view);
  g_assert (model);

  setting_model = TRUE;

  client = gconf_client_get_default ();

  current_gtk_theme = gconf_client_get_string (client, GTK_THEME_KEY, NULL);
  current_icon_theme = gconf_client_get_string (client, ICON_THEME_KEY, NULL);
  window_manager = gnome_wm_manager_get_current (gdk_display_get_default_screen (gdk_display_get_default ()));
  wm_settings.flags = GNOME_WM_SETTING_THEME;
  if (window_manager)
    {
      gnome_window_manager_get_settings (window_manager, &wm_settings);
      current_window_theme = g_strdup (wm_settings.theme);
    }
  else
    {
      current_window_theme = g_strdup (window_theme_default_name);
    }

  /* FIXME: What do we really do when there is no theme? */
  if (current_icon_theme == NULL)
    current_icon_theme = g_strdup (icon_theme_default_name);
  if (current_gtk_theme == NULL)
    current_gtk_theme = g_strdup (gtk_theme_default_name);

  /* handle first time */
  if (first_time)
    {
      initial_meta_theme_info.gtk_theme_name = g_strdup (current_gtk_theme);
      initial_meta_theme_info.icon_theme_name = g_strdup (current_icon_theme);
      initial_meta_theme_info.metacity_theme_name = g_strdup (current_window_theme);

      for (list = meta_theme_list; list; list = list->next)
	{
	  GnomeThemeMetaInfo *theme_info = list->data;

	  if ((theme_info->gtk_theme_name && !strcmp (theme_info->gtk_theme_name, gtk_theme_default_name)) &&
	      (theme_info->metacity_theme_name && !strcmp (theme_info->metacity_theme_name, window_theme_default_name)) &&
	      (theme_info->icon_theme_name && !strcmp (theme_info->icon_theme_name, icon_theme_default_name)))
	    {
	      meta_theme_default_name = g_strdup (theme_info->name);
	      break;
	    }
	}
    }
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
				  GTK_POLICY_NEVER, GTK_POLICY_NEVER);
  gtk_widget_set_usize (swindow, -1, -1);

  /* Sort meta_theme_list to be in the same order of the current data.  This way
   * we can walk through them together. */
  meta_theme_list = g_list_sort (meta_theme_list, sort_meta_theme_list_func);
  list = meta_theme_list;
  valid = gtk_tree_model_get_iter_first (model, &iter);

  while (valid || list != NULL)
    {
      GnomeThemeMetaInfo *list_meta_theme_info = NULL;
      GnomeThemeMetaInfo *model_meta_theme_info = NULL;
      gchar *blurb;
      gboolean list_is_default = FALSE;
      GdkPixbuf *pixbuf;

      /* Check info on the list */
      if (list)
	{
	  list_meta_theme_info = list->data;
	  if (meta_theme_default_name && strcmp (meta_theme_default_name, list_meta_theme_info->name) == 0)
	    list_is_default = TRUE;
	  else
	    list_is_default = FALSE;
	}

      if (valid)
	{
	  gtk_tree_model_get (model, &iter,
			      META_THEME_ID_COLUMN, &name,
			      META_THEME_FLAG_COLUMN, &flag,
			      -1);
	  if (flag & THEME_FLAG_CUSTOM)
	    model_meta_theme_info = &custom_meta_theme_info;
	  else if (flag & THEME_FLAG_INITIAL)
	    model_meta_theme_info = &initial_meta_theme_info;
	  else
	    model_meta_theme_info = gnome_theme_meta_info_find (name);
	  g_free (name);
	}
      
      /* start comparing values */
      if (list && valid)
	{
	  gint compare_val;

	  if (flag & THEME_FLAG_CUSTOM || flag & THEME_FLAG_INITIAL)
	    {
	      /* We can always skip the custom row, as it's never in the list */
	      valid = gtk_tree_model_iter_next (model, &iter);
	      i++;
	      goto end_of_loop;
	    }

	  compare_val = gnome_theme_manager_sort_func (model_meta_theme_info->readable_name, list_meta_theme_info->readable_name,
						       flag, list_is_default ? THEME_FLAG_DEFAULT : 0);

	  if (compare_val < 0)
	    {
	      GtkTreeIter iter_to_remove;

	      /* This item is no longer in the list
	       */
	      iter_to_remove = iter;
	      valid = gtk_tree_model_iter_next (model, &iter);
	      gtk_list_store_remove (GTK_LIST_STORE (model), &iter_to_remove);
	    }
	  else if (compare_val == 0)
	    {
	      /* We reset the blurb in case it has changed */
	      blurb = g_strdup_printf ("<span size=\"larger\" weight=\"bold\">%s</span>\n%s",
				       list_meta_theme_info->readable_name, list_meta_theme_info->comment);
	      pixbuf = default_image;

	      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				  META_THEME_PIXBUF_COLUMN, pixbuf,
				  META_THEME_NAME_COLUMN, blurb,
				  META_THEME_ID_COLUMN, list_meta_theme_info->name,
				  META_THEME_FLAG_COLUMN, flag,
				  -1);
	      g_free (blurb);

	      list = list->next;
	      valid = gtk_tree_model_iter_next (model, &iter);
	      i++;
	    }
	  else
	    {
	      /* we insert a new item */
	      GtkTreeIter iter_to_prepend;
	      gtk_list_store_insert_before (GTK_LIST_STORE (model), &iter_to_prepend, &iter);
	      blurb = g_strdup_printf ("<span size=\"larger\" weight=\"bold\">%s</span>\n%s",
				       list_meta_theme_info->readable_name, list_meta_theme_info->comment);
	      pixbuf = default_image;

	      gtk_list_store_set (GTK_LIST_STORE (model), &iter_to_prepend,
				  META_THEME_PIXBUF_COLUMN, pixbuf,
				  META_THEME_NAME_COLUMN, blurb,
				  META_THEME_ID_COLUMN, list_meta_theme_info->name,
				  META_THEME_FLAG_COLUMN, list_is_default ? THEME_FLAG_DEFAULT : 0,
				  -1);
	      g_free (blurb);

	      i++;
	      list = list->next;
	    }
	  goto end_of_loop;
	}
      else if (list)
	{
	  list_meta_theme_info = list->data;
	  gtk_list_store_append (GTK_LIST_STORE (model), &iter);

	  blurb = g_strdup_printf ("<span size=\"larger\" weight=\"bold\">%s</span>\n%s",
				   list_meta_theme_info->readable_name, list_meta_theme_info->comment);
	  if (i <= MAX_ELEMENTS_BEFORE_SCROLLING)
	    pixbuf = generate_theme_thumbnail (list_meta_theme_info, FALSE);
	  else
	    pixbuf = default_image;

	  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			      META_THEME_PIXBUF_COLUMN, pixbuf,
			      META_THEME_NAME_COLUMN, blurb,
			      META_THEME_ID_COLUMN, list_meta_theme_info->name,
			      META_THEME_FLAG_COLUMN, list_is_default ? THEME_FLAG_DEFAULT : 0,
			      -1);
	  g_free (blurb);

	  list = list->next;
	  i++;
	}
      else if (valid)
	{
	  /* It's a dead item. */
	  GtkTreeIter iter_to_remove;
	  iter_to_remove = iter;
	  valid = gtk_tree_model_iter_next (model, &iter);
	  gtk_list_store_remove (GTK_LIST_STORE (model), &iter_to_remove);
	}

    end_of_loop:
      if (i == MAX_ELEMENTS_BEFORE_SCROLLING)
	{
	  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swindow),
					  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	}
    }

  add_pixbuf_idle ();

  g_free (current_gtk_theme);
  g_free (current_icon_theme);
  g_free (current_window_theme);
  first_time = FALSE;
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
  if (window_manager)
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
      g_free (meta_theme_name);
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
  gboolean have_meta_theme;

  theme_list = gnome_theme_meta_info_find_all ();
  if (theme_list == NULL)
    {
      have_meta_theme = FALSE;
    }
  else
    {
      have_meta_theme = TRUE;
      gtk_widget_show (WID ("meta_theme_hbox"));
      load_meta_themes (GTK_TREE_VIEW (WID ("meta_theme_treeview")), theme_list);
    }
  g_list_free (theme_list);

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
  GdkPixbuf *pixbuf;

  dialog = gnome_theme_manager_get_theme_dialog ();
  tree_view = WID ("meta_theme_treeview");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));

  g_free (custom_meta_theme_info.gtk_theme_name);
  custom_meta_theme_info.gtk_theme_name = g_strdup (current_gtk_theme);
  g_free (custom_meta_theme_info.metacity_theme_name);
  custom_meta_theme_info.metacity_theme_name = g_strdup (current_window_theme);
  g_free (custom_meta_theme_info.icon_theme_name);
  custom_meta_theme_info.icon_theme_name = g_strdup (current_icon_theme);
  g_free (custom_meta_theme_info.name);
  custom_meta_theme_info.name = g_strdup ("__Custom Theme__");

  for (valid = gtk_tree_model_get_iter_first (model, &iter);
       valid;
       valid = gtk_tree_model_iter_next (model, &iter))
    {
      guint theme_flags = 0;

      gtk_tree_model_get (model, &iter,
			  META_THEME_FLAG_COLUMN, &theme_flags,
			  -1);
      if (theme_flags & THEME_FLAG_CUSTOM || theme_flags & THEME_FLAG_INITIAL)
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

  /* Invalidate the cache because the custom theme has potentially changed */
  /* Commented out because it does odd things */
  /*theme_thumbnail_invalidate_cache (&custom_meta_theme_info);*/

  pixbuf = generate_theme_thumbnail (&custom_meta_theme_info, TRUE);
  if (pixbuf == NULL)
    pixbuf = default_image;
  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		      META_THEME_PIXBUF_COLUMN, pixbuf,
		      META_THEME_NAME_COLUMN, blurb,
		      META_THEME_FLAG_COLUMN, THEME_FLAG_CUSTOM,
		      -1);

  gtk_widget_set_sensitive (WID ("meta_theme_save_button"), TRUE);
  path = gtk_tree_model_get_path (model, &iter);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree_view), path, NULL, FALSE);
  gtk_tree_path_free (path);
  g_free (blurb);
}


static void
add_initial_row_to_meta_theme (void)
{
  GladeXML *dialog;
  GtkWidget *tree_view;
  GtkTreeModel *model;
  GtkTreePath *path;
  GtkTreeIter iter;
  gboolean valid;
  gchar *blurb;
  GdkPixbuf *pixbuf;

  dialog = gnome_theme_manager_get_theme_dialog ();
  tree_view = WID ("meta_theme_treeview");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));

  for (valid = gtk_tree_model_get_iter_first (model, &iter);
       valid;
       valid = gtk_tree_model_iter_next (model, &iter))
    {
      guint theme_flags = 0;

      gtk_tree_model_get (model, &iter,
			  META_THEME_FLAG_COLUMN, &theme_flags,
			  -1);
      if (theme_flags & THEME_FLAG_CUSTOM || theme_flags & THEME_FLAG_INITIAL)
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

  /* Invalidate the cache because the custom theme has potentially changed */
  /* Commented out because it does odd things */
  /*theme_thumbnail_invalidate_cache (&custom_meta_theme_info);*/

  pixbuf = generate_theme_thumbnail (&initial_meta_theme_info, TRUE);
  if (pixbuf == NULL)
    pixbuf = default_image;

  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
		      META_THEME_PIXBUF_COLUMN, pixbuf,
		      META_THEME_NAME_COLUMN, blurb,
		      META_THEME_FLAG_COLUMN, THEME_FLAG_INITIAL,
		      -1);

  gtk_widget_set_sensitive (WID ("meta_theme_save_button"), TRUE);
  path = gtk_tree_model_get_path (model, &iter);
  gtk_tree_path_free (path);
  g_free (blurb);
}


static void
remove_custom_row_from_meta_theme (void)
{
  GladeXML *dialog;
  GtkWidget *tree_view;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GtkTreeIter next_iter;
  gboolean valid;

  dialog = gnome_theme_manager_get_theme_dialog ();
  tree_view = WID ("meta_theme_treeview");
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (tree_view));

  valid = gtk_tree_model_get_iter_first (model, &iter);
  while (valid)
    {
      guint theme_flags = 0;

      next_iter = iter;
      valid = gtk_tree_model_iter_next (model, &next_iter);

      gtk_tree_model_get (model, &iter,
			  META_THEME_FLAG_COLUMN, &theme_flags,
			  -1);

      if (theme_flags & THEME_FLAG_CUSTOM || theme_flags & THEME_FLAG_INITIAL)
	{
	  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
	}
      iter = next_iter;
    }
  g_free (custom_meta_theme_info.gtk_theme_name);
  g_free (custom_meta_theme_info.metacity_theme_name);
  g_free (custom_meta_theme_info.icon_theme_name);
  g_free (custom_meta_theme_info.name);

  gtk_widget_set_sensitive (WID ("meta_theme_save_button"), FALSE);

  custom_meta_theme_info.gtk_theme_name = NULL;
  custom_meta_theme_info.metacity_theme_name = NULL;
  custom_meta_theme_info.icon_theme_name = NULL;
  custom_meta_theme_info.name = NULL;
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
  static gboolean first_time_run = TRUE;

  client = gconf_client_get_default ();

  /* Get the settings */
  current_gtk_theme = gconf_client_get_string (client, GTK_THEME_KEY, NULL);
  current_icon_theme = gconf_client_get_string (client, ICON_THEME_KEY, NULL);
  window_manager = gnome_wm_manager_get_current (gdk_display_get_default_screen (gdk_display_get_default ()));
  wm_settings.flags = GNOME_WM_SETTING_THEME;
  if (window_manager) {
    gnome_window_manager_get_settings (window_manager, &wm_settings);
    current_window_theme = g_strdup (wm_settings.theme);
  } else
    current_window_theme = g_strdup ("");

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
      else if (row_theme_flags & THEME_FLAG_INITIAL)
	{
	  meta_theme_info = &initial_meta_theme_info;
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

  if (custom_theme_found && first_time_run)
    {
      initial_meta_theme_set = TRUE;
      initial_meta_theme_info.name = g_strdup ("__Initial Theme__");
      initial_meta_theme_info.gtk_theme_name = g_strdup (current_gtk_theme);
      initial_meta_theme_info.metacity_theme_name = g_strdup (current_window_theme);
      initial_meta_theme_info.icon_theme_name = g_strdup (current_icon_theme);
    }
  first_time_run = FALSE;
  

  if (custom_theme_found)
    {
      add_custom_row_to_meta_theme (current_gtk_theme, current_window_theme, current_icon_theme);
    }
  else if (initial_meta_theme_set)
    {
      add_initial_row_to_meta_theme ();
    }      
  else
    {
      remove_custom_row_from_meta_theme ();
    }
  g_free (current_gtk_theme);
  g_free (current_window_theme);
  g_free (current_icon_theme);
  add_pixbuf_idle ();
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
update_font_button_state (GladeXML *dialog)
{
  GConfClient *client = gconf_client_get_default ();
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (WID ("meta_theme_treeview")));
  
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      GnomeThemeMetaInfo *meta_theme_info;
      char *meta_theme_name, *str;
      
      gtk_tree_model_get (model, &iter,
			  META_THEME_ID_COLUMN, &meta_theme_name,
			  -1);
      if (!meta_theme_name)
	return;
      
      meta_theme_info = gnome_theme_meta_info_find (meta_theme_name);

      g_assert (meta_theme_info);
      g_free (meta_theme_name);

      str = gconf_client_get_string (client, FONT_KEY, NULL);
      
      if (meta_theme_info->application_font != NULL && str != NULL &&
	  strcmp (meta_theme_info->application_font, str) == 0)
	{
	  gtk_widget_set_sensitive (WID ("meta_theme_font1_button"), FALSE);
	  gtk_widget_set_sensitive (WID ("meta_theme_font2_button"), FALSE);
	}
      else
	{
	  gtk_widget_set_sensitive (WID ("meta_theme_font1_button"), TRUE);
	  gtk_widget_set_sensitive (WID ("meta_theme_font2_button"), TRUE);
	}

      g_free (str);
    }  
  
}

static void
font_key_changed (GConfClient        *client,
		  guint               cnxn_id,
		  GConfEntry         *entry,
		  gpointer            user_data)
{
  GladeXML *dialog = user_data;

  update_font_button_state (dialog);
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

  update_themes_from_disk (dialog);
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
  gtk_tree_view_set_model (tree_view, model);
  selection = gtk_tree_view_get_selection (tree_view);
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
  g_signal_connect (G_OBJECT (selection), "changed", changed_callback, dialog);
}


static void
gnome_meta_theme_installer_run_cb (GtkWidget *button,
				   GtkWidget *parent_window)
{
  gnome_theme_installer_run (parent_window, NULL, FALSE);
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
apply_font_clicked (GtkWidget *button,
		    gpointer   data)
{
  GladeXML *dialog = data;
  GConfClient *client;
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  
  client = gconf_client_get_default ();
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (WID ("meta_theme_treeview")));
  
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      GnomeThemeMetaInfo *meta_theme_info;
      char *meta_theme_name;
      
      gtk_tree_model_get (model, &iter,
			  META_THEME_ID_COLUMN, &meta_theme_name,
			  -1);
      meta_theme_info = gnome_theme_meta_info_find (meta_theme_name);

      g_assert (meta_theme_info);
      g_free (meta_theme_name);

      gconf_client_set_string (client, FONT_KEY, meta_theme_info->application_font, NULL);
    }  
}
	    
	    
static void
setup_dialog (GladeXML *dialog)
{
  GConfClient *client;
  GtkWidget *parent, *widget;
  GnomeWindowManager *window_manager;
  GtkSizeGroup *size_group;

  default_image = gdk_pixbuf_new_from_file(GNOMECC_DATA_DIR "/pixmaps/theme-thumbnailing.png", NULL);

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

  g_signal_connect (G_OBJECT (WID ("meta_theme_install_button")), "clicked", G_CALLBACK (gnome_meta_theme_installer_run_cb), parent);

  g_signal_connect (G_OBJECT (WID ("meta_theme_details_button")), "clicked", gnome_theme_details_show, NULL);

  g_signal_connect (G_OBJECT (WID ("meta_theme_font1_button")), "clicked", G_CALLBACK (apply_font_clicked), dialog);

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

  gconf_client_notify_add (client,
			   FONT_KEY,
			   (GConfClientNotifyFunc) &font_key_changed,
			   dialog, NULL, NULL);
  if (window_manager)
    g_signal_connect (G_OBJECT (window_manager),
		      "settings_changed",
		      (GCallback) window_settings_changed, dialog);

  update_themes_from_disk (dialog);
  gtk_widget_grab_focus (WID ("meta_theme_treeview"));
  gnome_theme_info_register_theme_change (theme_changed_func, dialog);



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

  update_font_button_state (dialog);
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
gnome_theme_manager_sort_func (const gchar *a_str,
			       const gchar *b_str,
			       guint        a_flag,
			       guint        b_flag)
{
  gint retval;

  if (a_flag & (THEME_FLAG_CUSTOM | THEME_FLAG_INITIAL))
    retval = -1;
  else if (b_flag & (THEME_FLAG_CUSTOM | THEME_FLAG_INITIAL))
    retval = 1;
  else if (a_flag & THEME_FLAG_DEFAULT)
    retval = -1;
  else if (b_flag & THEME_FLAG_DEFAULT)
    retval = 1;
  else
    {
      retval = g_utf8_collate (a_str?a_str:"",
			       b_str?b_str:"");
    }
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

/* Starts nautilus on the icon themes directory*/
void
gnome_theme_manager_icon_show_manage_themes (GtkWidget *button, gpointer data)
{
	gchar *path, *command;
	GnomeVFSURI *uri;

	path = g_strdup_printf ("%s/.icons", g_get_home_dir ());
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

		if (gnome_vfs_uri_is_local (uri))
			filename = gnome_vfs_unescape_string (
					gnome_vfs_uri_get_path (uri),
					G_DIR_SEPARATOR_S);
		else
			filename = gnome_vfs_unescape_string (
					gnome_vfs_uri_to_string (uri, GNOME_VFS_URI_HIDE_NONE),
					G_DIR_SEPARATOR_S);

		gnome_vfs_uri_list_unref (uris);
	}

	gnome_theme_installer_run (widget, filename, FALSE);
	g_free (filename);
}


static gchar *
get_default_string_from_key (const char *key)
{
  GConfClient *client;
  GConfValue *value;
  GError *error = NULL;
  gchar *str = NULL;

  client = gconf_client_get_default ();
  value = gconf_client_get_default_from_schema (client, key, &error);

  if (error)
    {
      g_clear_error (&error);
      return NULL;
    }

  if (value)
    {
      if (value->type == GCONF_VALUE_STRING)
	str = gconf_value_to_string (value);
      gconf_value_free (value);
    }

  return str;
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

  gnome_program_init ("gnome-theme-manager", VERSION,
		      LIBGNOMEUI_MODULE, argc, argv,
		      GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
		      NULL);

  gtk_theme_default_name = get_default_string_from_key (GTK_THEME_KEY);
  window_theme_default_name = get_default_string_from_key (METACITY_THEME_KEY);
  icon_theme_default_name = get_default_string_from_key (ICON_THEME_KEY);

  if (gtk_theme_default_name == NULL ||
      window_theme_default_name == NULL ||
      icon_theme_default_name == NULL)
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (NULL,
				       GTK_DIALOG_MODAL,
				       GTK_MESSAGE_ERROR,
				       GTK_BUTTONS_OK,
				       _("The default theme schemas could not be found on your system.  This means that you probably don't have metacity installed, or that your gconf is configured incorrectly."));
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      exit (0);
    }

  gnome_theme_init (NULL);

  gnome_wm_manager_init ();
  activate_settings_daemon ();

  dialog = gnome_theme_manager_get_theme_dialog ();

  setup_dialog (dialog);

  gtk_main ();

  return 0;
}
