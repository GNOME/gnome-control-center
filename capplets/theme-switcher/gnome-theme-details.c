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
#include "gtkrc-utils.h"

#define MAX_ELEMENTS_BEFORE_SCROLLING 12

static gboolean theme_details_initted = FALSE;
static gboolean setting_model = FALSE;

enum {
	THEME_GTK,
	THEME_WINDOW,
	THEME_ICON
};
	

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
static void update_color_buttons_from_string (gchar *color_scheme);
static void color_scheme_changed            (GObject           *settings,
                                             GParamSpec        *pspec,
                                             gpointer           data);

void revert_color_scheme_key (GtkWidget *checkbutton, gpointer *data);

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
    capplet_help (GTK_WINDOW (dialog), "user-guide.xml", "goscustdesk-12");
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
  g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
}

static void
gtk_theme_update_remove_button (GtkTreeSelection *selection, 
				GtkWidget *remove_button,
				gint theme_type)
{
  gchar *theme_selected;
  GtkTreeModel *model;
  GtkTreeIter iter;
  GList *theme_list=NULL, *list;
  gchar *theme_base=NULL;
  GnomeVFSResult vfs_result;
  GnomeVFSFileInfo *vfs_info;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
     gtk_tree_model_get (model, &iter,
			  THEME_NAME_COLUMN, &theme_selected,
			  -1);
  else
    theme_selected = NULL;

  if (theme_selected != NULL) 
  {
     switch (theme_type) {
		case THEME_GTK: theme_base = g_strdup("/gtk-2.0/");
				theme_list = gnome_theme_info_find_by_type (GNOME_THEME_GTK_2);
				break;
		case THEME_ICON: theme_list = gnome_theme_icon_info_find_all();
				 break;
		case THEME_WINDOW: theme_base = g_strdup("/metacity-1/");
				    theme_list = gnome_theme_info_find_by_type (GNOME_THEME_METACITY);
				    break;
		default: theme_list = NULL;
			 break;
    }
  }
		
  vfs_info = gnome_vfs_file_info_new ();

  for (list = theme_list; list; list = list->next)
  {
    GnomeThemeInfo *info = list->data;
	gchar *theme_dir = NULL;

    if (!strcmp(info->name, theme_selected))
    {
	if (theme_type == THEME_ICON) 
		theme_dir = g_strdup(info->path);
	else 
		theme_dir = g_strdup_printf("%s/%s", info->path, theme_base);
			
	vfs_result = gnome_vfs_get_file_info (theme_dir,
					      vfs_info,
				              GNOME_VFS_FILE_INFO_GET_ACCESS_RIGHTS);
	g_free (theme_dir);

        if (vfs_result == GNOME_VFS_OK) 
	{
		if ((vfs_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_ACCESS) &&
		    !(vfs_info->permissions & GNOME_VFS_PERM_ACCESS_WRITABLE)) 
			gtk_widget_set_sensitive(remove_button, FALSE); 
		else 
			gtk_widget_set_sensitive(remove_button, TRUE); 
	} else {
		gtk_widget_set_sensitive(remove_button, FALSE);
	}

      }
    }
    gnome_vfs_file_info_unref(vfs_info);
    g_free (theme_base);
    g_free (theme_selected);
}

static void
update_color_scheme_tab (const gchar *theme)
{
	GSList *symbolic_colors = NULL;
	GSList *engines = NULL;
	gboolean fg, bg, base, text, fg_s, bg_s, enable_colors;
	gchar *filename;
  gchar *theme_name = NULL;
	GtkSettings *settings;
	GladeXML *dialog;

	dialog = gnome_theme_manager_get_theme_dialog ();

	if (theme == NULL) {
		settings = gtk_settings_get_default ();
		g_object_get (G_OBJECT (settings), "gtk-theme-name", &theme_name, NULL);
		theme = theme_name;
	}
	filename = gtkrc_find_named (theme);


	settings = gtk_settings_get_default ();
	g_object_get (G_OBJECT (settings), "gtk-theme-name", &theme_name, NULL);
	filename = gtkrc_find_named (theme_name);

	gtkrc_get_details (filename, &engines, &symbolic_colors);
	fg = (g_slist_find_custom (symbolic_colors, "fg_color", g_str_equal) != NULL);
	bg = (g_slist_find_custom (symbolic_colors, "bg_color", g_str_equal) != NULL);
	base = (g_slist_find_custom (symbolic_colors, "base_color", g_str_equal) != NULL);
	text = (g_slist_find_custom (symbolic_colors, "text_color", g_str_equal) != NULL);
	fg_s = (g_slist_find_custom (symbolic_colors, "selected_fg_color", g_str_equal) != NULL);
	bg_s = (g_slist_find_custom (symbolic_colors, "selected_bg_color", g_str_equal) != NULL);

	enable_colors = (fg && bg && base && text && fg_s && bg_s);

	gtk_widget_set_sensitive (WID ("color_scheme_table"), enable_colors);
	gtk_widget_set_sensitive (WID ("color_scheme_revert_button"), enable_colors);

	if (enable_colors)
		gtk_widget_hide (WID ("color_scheme_message_hbox"));
	else
		gtk_widget_show (WID ("color_scheme_message_hbox"));

	g_free (filename);
	g_free (theme_name);
}

static void
gtk_theme_selection_changed (GtkTreeSelection *selection,
			     gpointer          data)
{
  GladeXML *dialog;
  GtkTreeModel *model;
  GtkTreeIter iter;

  dialog = gnome_theme_manager_get_theme_dialog ();

  update_gconf_key_from_selection (selection, GTK_THEME_KEY);
  gtk_theme_update_remove_button(selection, WID("control_remove_button"), THEME_GTK);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gchar *theme;

      gtk_tree_model_get (model, &iter,
                          THEME_ID_COLUMN, &theme,
                          -1);
      update_color_scheme_tab (theme);
      g_free (theme);
    }
}

static void
window_theme_selection_changed (GtkTreeSelection *selection,
				gpointer          data)
{
  GnomeWindowManager *window_manager = NULL;
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
  if (window_manager != NULL && strcmp (gnome_window_manager_get_name (window_manager), "No name")) {
    wm_settings.flags = GNOME_WM_SETTING_THEME;
    wm_settings.theme = window_theme_name;
    gnome_window_manager_change_settings (window_manager, &wm_settings);
  }
  g_free (window_theme_name);
}

static void
icon_theme_selection_changed (GtkTreeSelection *selection,
			     gpointer          data)
{
  GladeXML *dialog;
		
  dialog = gnome_theme_manager_get_theme_dialog ();
  update_gconf_key_from_selection (selection, ICON_THEME_KEY);
  gtk_theme_update_remove_button(selection, WID("icon_remove_button"), THEME_ICON);
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
  gtk_widget_set_size_request (swindow, -1, -1);

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
	  gtk_widget_set_size_request (swindow, -1, rectangle.height);
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

static void
remove_theme(GtkWidget *button, gpointer data)
{
  GladeXML *dialog;
  GtkWidget *confirm_dialog, *info_dialog;
  GList *theme_list, *string_list, *list;
  GtkTreeSelection *selection;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gchar *theme_dir, *theme_selected, *treeview;
  gint response, theme_type;
  GList *uri_list;
  GnomeVFSResult result;
  
  confirm_dialog = gtk_message_dialog_new (NULL,
		          		   GTK_DIALOG_MODAL,
					   GTK_MESSAGE_QUESTION,
					   GTK_BUTTONS_OK_CANCEL,
					   _("Would you like to remove this theme?"));
  response = gtk_dialog_run (GTK_DIALOG (confirm_dialog));
  gtk_widget_destroy(confirm_dialog);
  if (response == GTK_RESPONSE_CANCEL)
	return;

  theme_type = (gint)data;
  switch (theme_type) {
	case THEME_GTK: theme_dir = g_strdup("/gtk-2.0/");
               		theme_list = gnome_theme_info_find_by_type (GNOME_THEME_GTK_2);
			treeview=g_strdup("control_theme_treeview");
                        break;
	case THEME_ICON: theme_list = gnome_theme_icon_info_find_all();
			 treeview=g_strdup("icon_theme_treeview");
		 	 theme_dir = NULL;
			 break;
	case THEME_WINDOW: theme_dir = g_strdup("/metacity-1/");
 			   theme_list = gnome_theme_info_find_by_type (GNOME_THEME_METACITY);
			   treeview=g_strdup("window_theme_treeview");
			   break;
	default: theme_list = NULL;
		 theme_dir = NULL;
		 treeview = NULL;
		 break;
  }


  if (treeview != NULL) {
	  dialog = gnome_theme_manager_get_theme_dialog ();
	  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(WID(treeview)));
	  if (gtk_tree_selection_get_selected (selection, &model, &iter))
  	  {
	  	gtk_tree_model_get (model, &iter,
				      THEME_NAME_COLUMN, &theme_selected,
				      -1);
	  } else {
		theme_selected = NULL;
  	  }

  	if (theme_selected!=NULL)
  	{
		string_list = NULL;

		for (list = theme_list; list; list = list->next)
		{
			GnomeThemeInfo *info = list->data;
			if (!strcmp(info->name, theme_selected))
			{

				if (theme_type == THEME_ICON)
					theme_dir = g_strdup(g_path_get_dirname(info->path));
				else
					theme_dir = g_strdup_printf("%s/%s", info->path, theme_dir);
	
				uri_list = g_list_prepend(NULL, gnome_vfs_uri_new (theme_dir));

				result = gnome_vfs_xfer_delete_list (uri_list, GNOME_VFS_XFER_ERROR_MODE_ABORT,
								     GNOME_VFS_XFER_RECURSIVE,
								     NULL, NULL);
				if (result == GNOME_VFS_OK) 
				{
					info_dialog = gtk_message_dialog_new (NULL,
					                        GTK_DIALOG_MODAL,
								GTK_MESSAGE_INFO,
								GTK_BUTTONS_OK_CANCEL,
								_("Theme deleted succesfully. Please select another theme."));
					gtk_list_store_remove (GTK_LIST_STORE(model), &iter);
					gtk_dialog_run (GTK_DIALOG (info_dialog));

				} else {
					info_dialog = gtk_message_dialog_new (NULL,
								GTK_DIALOG_MODAL,
								GTK_MESSAGE_ERROR,
								GTK_BUTTONS_OK,
								_("Theme can not be deleted"));
					gtk_dialog_run (GTK_DIALOG (info_dialog));
				}
				gtk_widget_destroy (info_dialog);
				gnome_vfs_uri_list_free (uri_list);
			}
		}
  	}
  }
}

static void
gtk_color_scheme_changed (GtkWidget *colorbutton, GladeXML *dialog)
{
  GConfClient *client = NULL;
  gchar *new_scheme;
  GdkColor colors[6];
  gchar *bg, *fg, *text, *base, *selected_fg, *selected_bg;
  GtkWidget *widget;

  widget = WID ("fg_colorbutton");
  gtk_color_button_get_color (GTK_COLOR_BUTTON (widget), &colors[0]);
  widget = WID ("bg_colorbutton");
  gtk_color_button_get_color (GTK_COLOR_BUTTON (widget), &colors[1]);
  widget = WID ("text_colorbutton");
  gtk_color_button_get_color (GTK_COLOR_BUTTON (widget), &colors[2]);
  widget = WID ("base_colorbutton");
  gtk_color_button_get_color (GTK_COLOR_BUTTON (widget), &colors[3]);
  widget = WID ("selected_fg_colorbutton");
  gtk_color_button_get_color (GTK_COLOR_BUTTON (widget), &colors[4]);
  widget = WID ("selected_bg_colorbutton");
  gtk_color_button_get_color (GTK_COLOR_BUTTON (widget), &colors[5]);

  fg = g_strdup_printf ("fg_color:#%04x%04x%04x\n", colors[0].red, colors[0].green, colors[0].blue);
  bg = g_strdup_printf ("bg_color:#%04x%04x%04x\n", colors[1].red, colors[1].green, colors[1].blue);
  text = g_strdup_printf ("text_color:#%04x%04x%04x\n", colors[2].red, colors[2].green, colors[2].blue);
  base = g_strdup_printf ("base_color:#%04x%04x%04x\n", colors[3].red, colors[3].green, colors[3].blue);
  selected_fg = g_strdup_printf ("selected_fg_color:#%04x%04x%04x\n", colors[4].red, colors[4].green, colors[4].blue);
  selected_bg = g_strdup_printf ("selected_bg_color:#%04x%04x%04x", colors[5].red, colors[5].green, colors[5].blue);

  new_scheme = g_strconcat (fg, bg, text, base, selected_fg, selected_bg, NULL);

  client = gconf_client_get_default ();

  /* Currently we assume this has only been called when one of the colours has
   * actually changed, so we don't check the original key first
   */
  gconf_client_set_string (client, COLOR_SCHEME_KEY, new_scheme, NULL);

  g_object_unref (G_OBJECT (client));
  free_all (fg, bg, text, base, selected_fg, selected_bg, new_scheme, NULL);
}

void
revert_color_scheme_key (GtkWidget *checkbutton, gpointer *data)
{
  GConfClient *client = NULL;
  GladeXML *dialog;

  dialog = gnome_theme_manager_get_theme_dialog ();

  client = gconf_client_get_default ();
  gconf_client_set_string (client, COLOR_SCHEME_KEY, "", NULL);
  g_object_unref (G_OBJECT (client));
}

void
gnome_theme_details_init (void)
{
  GtkWidget *parent, *widget;
  GladeXML *dialog;
  gchar *theme;

  if (theme_details_initted)
    return;
  theme_details_initted = TRUE;

  dialog = gnome_theme_manager_get_theme_dialog ();
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
  widget = WID ("control_remove_button");
  g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (remove_theme), (gint*)THEME_GTK);
  widget = WID ("control_install_button");
  g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (gnome_theme_installer_run_cb), parent);

  /* window manager themes */
  widget = WID ("window_remove_button");
  g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (remove_theme), (gint*)THEME_WINDOW);
  widget = WID ("window_install_button");
  g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (gnome_theme_installer_run_cb), parent);

  /* icon themes */
  widget = WID ("icon_remove_button");
  g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (remove_theme), (gint*)THEME_ICON);
  widget = WID ("icon_install_button");
  g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (gnome_theme_installer_run_cb), parent);

  /* color preferences */
  widget = WID ("fg_colorbutton");
  g_signal_connect (G_OBJECT (widget), "color_set", G_CALLBACK (gtk_color_scheme_changed), dialog);
  widget = WID ("bg_colorbutton");
  g_signal_connect (G_OBJECT (widget), "color_set", G_CALLBACK (gtk_color_scheme_changed), dialog);
  widget = WID ("text_colorbutton");
  g_signal_connect (G_OBJECT (widget), "color_set", G_CALLBACK (gtk_color_scheme_changed), dialog);
  widget = WID ("base_colorbutton");
  g_signal_connect (G_OBJECT (widget), "color_set", G_CALLBACK (gtk_color_scheme_changed), dialog);
  widget = WID ("selected_fg_colorbutton");
  g_signal_connect (G_OBJECT (widget), "color_set", G_CALLBACK (gtk_color_scheme_changed), dialog);
  widget = WID ("selected_bg_colorbutton");
  g_signal_connect (G_OBJECT (widget), "color_set", G_CALLBACK (gtk_color_scheme_changed), dialog);

  widget = WID ("color_scheme_revert_button");
  g_signal_connect (G_OBJECT (widget), "clicked", G_CALLBACK (revert_color_scheme_key), parent);

  g_object_get (G_OBJECT (gtk_settings_get_default()), "gtk-color-scheme", &theme, NULL);
  update_color_buttons_from_string (theme);

  g_signal_connect (gtk_settings_get_default(), "notify::gtk-color-scheme", 
                    G_CALLBACK (color_scheme_changed),  NULL);

  /* general signals */
  g_signal_connect (G_OBJECT (parent), "response", G_CALLBACK (cb_dialog_response), NULL);
  g_signal_connect (G_OBJECT (parent), "delete-event", G_CALLBACK (gtk_true), NULL);

  gtk_drag_dest_set (parent, GTK_DEST_DEFAULT_ALL,
		     drop_types, n_drop_types,
		     GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_MOVE);
  g_signal_connect (G_OBJECT (parent), "drag-motion", G_CALLBACK (gnome_theme_manager_drag_motion_cb), NULL);
  g_signal_connect (G_OBJECT (parent), "drag-leave", G_CALLBACK (gnome_theme_manager_drag_leave_cb), NULL);
  g_signal_connect (G_OBJECT (parent), "drag-data-received", G_CALLBACK (gnome_theme_manager_drag_data_received_cb), NULL);

  capplet_set_icon (parent, "gnome-settings-theme");

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

  gboolean have_gtk_theme;
  gboolean have_window_theme;
  gboolean have_icon_theme;

  
  gnome_theme_details_init ();

  dialog = gnome_theme_manager_get_theme_dialog ();
  window_manager = gnome_wm_manager_get_current (gdk_display_get_default_screen (gdk_display_get_default ()));

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
      gtk_widget_hide (WID ("control_theme_hbox"));
    }
  else
    {
      have_gtk_theme = TRUE;
      gtk_widget_show (WID ("control_theme_hbox"));
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
      gtk_widget_hide (WID ("window_theme_hbox"));
    }
  else
    {
      have_window_theme = TRUE;
      gtk_widget_show (WID ("window_theme_hbox"));
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
      gtk_widget_hide (WID ("icon_theme_hbox"));
    }
  else
    {
      have_icon_theme = TRUE;
      gtk_widget_show (WID ("icon_theme_hbox"));
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
	  GtkTreePath *cursor_path;
	  gboolean cursor_same = FALSE;

	  gtk_tree_view_get_cursor (GTK_TREE_VIEW (tree_view), &cursor_path, NULL);
	  path = gtk_tree_model_get_path (model, &iter);
	  if (cursor_path && gtk_tree_path_compare (path, cursor_path) == 0)
	    cursor_same = TRUE;

	  gtk_tree_path_free (cursor_path);

	  if (!cursor_same)
	    {
	      gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree_view), path, NULL, FALSE);
	    }
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

static void
update_color_buttons_from_string (gchar *color_scheme)
{
  GdkColor color_scheme_colors[6];
  gchar **color_scheme_strings, **color_scheme_pair, *current_string;
  gint i;
  GtkWidget *widget;
  GladeXML *dialog;

  if (!color_scheme) return;
  if (!strcmp (color_scheme, "")) return;

  dialog = gnome_theme_manager_get_theme_dialog ();

  /* The color scheme string consists of name:color pairs, seperated by
   * newlines, so first we split the string up by new line */

  color_scheme_strings = g_strsplit (color_scheme, "\n", 0);

  /* loop through the name:color pairs, and save the colour if we recognise the name */
  i = 0;
  while ((current_string = color_scheme_strings[i++]))
  {
    color_scheme_pair = g_strsplit (current_string, ":", 0);

    if (color_scheme_pair[0] == NULL)
      continue;

    g_strstrip (color_scheme_pair[0]);
    g_strstrip (color_scheme_pair[1]);


    if (!strcmp ("fg_color", color_scheme_pair[0]))
      gdk_color_parse (color_scheme_pair[1], &color_scheme_colors[0]);
    else if (!strcmp ("bg_color", color_scheme_pair[0]))
      gdk_color_parse (color_scheme_pair[1], &color_scheme_colors[1]);
    else if (!strcmp ("text_color", color_scheme_pair[0]))
      gdk_color_parse (color_scheme_pair[1], &color_scheme_colors[2]);
    else if (!strcmp ("base_color", color_scheme_pair[0]))
      gdk_color_parse (color_scheme_pair[1], &color_scheme_colors[3]);
    else if (!strcmp ("selected_fg_color", color_scheme_pair[0]))
      gdk_color_parse (color_scheme_pair[1], &color_scheme_colors[4]);
    else if (!strcmp ("selected_bg_color", color_scheme_pair[0]))
      gdk_color_parse (color_scheme_pair[1], &color_scheme_colors[5]);

    g_strfreev (color_scheme_pair);
  }

  g_strfreev (color_scheme_strings);

  /* not sure whether we need to do this, but it can't hurt */
  for (i = 0; i < 6; i++)
    gdk_colormap_alloc_color (gdk_colormap_get_system (), &color_scheme_colors[i], FALSE, TRUE);

  /* now set all the buttons to the correct settings */
  widget = WID ("fg_colorbutton");
  gtk_color_button_set_color (GTK_COLOR_BUTTON (widget), &color_scheme_colors[0]);
  widget = WID ("bg_colorbutton");
  gtk_color_button_set_color (GTK_COLOR_BUTTON (widget), &color_scheme_colors[1]);
  widget = WID ("text_colorbutton");
  gtk_color_button_set_color (GTK_COLOR_BUTTON (widget), &color_scheme_colors[2]);
  widget = WID ("base_colorbutton");
  gtk_color_button_set_color (GTK_COLOR_BUTTON (widget), &color_scheme_colors[3]);
  widget = WID ("selected_fg_colorbutton");
  gtk_color_button_set_color (GTK_COLOR_BUTTON (widget), &color_scheme_colors[4]);
  widget = WID ("selected_bg_colorbutton");
  gtk_color_button_set_color (GTK_COLOR_BUTTON (widget), &color_scheme_colors[5]);


}

void
gnome_theme_details_update_from_gconf (void)
{
  GConfClient *client;
  GladeXML *dialog;
  GtkWidget *tree_view;
  gchar *theme = NULL;
  GnomeWindowManager *window_manager = NULL;
  GnomeWMSettings wm_settings;

  gnome_theme_details_init ();

  window_manager = gnome_wm_manager_get_current (gdk_display_get_default_screen (gdk_display_get_default ()));

  client = gconf_client_get_default ();
  dialog = gnome_theme_manager_get_theme_dialog ();

  /* FIXME: What do we really do when there is no theme?  Ask Havoc here. */
  tree_view = WID ("control_theme_treeview");
  theme = gconf_client_get_string (client, GTK_THEME_KEY, NULL);
  update_list_something (tree_view, theme);
  update_color_scheme_tab (theme);
  g_free (theme);

  tree_view = WID ("window_theme_treeview");
  wm_settings.flags = GNOME_WM_SETTING_THEME;
  if (window_manager != NULL && strcmp (gnome_window_manager_get_name (window_manager), "No name")) {
    gnome_window_manager_get_settings (window_manager, &wm_settings);
    update_list_something (tree_view, wm_settings.theme);
  }

  tree_view = WID ("icon_theme_treeview");
  theme = gconf_client_get_string (client, ICON_THEME_KEY, NULL);
  update_list_something (tree_view, theme);
  g_free (theme);

  /* update colour scheme tab */
  theme = gconf_client_get_string (client, COLOR_SCHEME_KEY, NULL);
  update_color_buttons_from_string (theme);
  g_free (theme);

  g_object_unref (client);
}

static void
color_scheme_changed (GObject    *settings,
                      GParamSpec *pspec,
                      gpointer    data)
{
  GConfClient *client;
  gchar *theme;

  client = gconf_client_get_default ();
  theme = gconf_client_get_string (client, COLOR_SCHEME_KEY, NULL);
  if (theme == NULL || strcmp (theme, "") == 0)
    g_object_get (settings, "gtk-color-scheme", &theme, NULL);

  update_color_buttons_from_string (theme);
  g_free (theme);
  g_object_unref (client);
}
