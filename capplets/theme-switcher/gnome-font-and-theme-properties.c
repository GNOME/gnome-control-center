/* This program was written with lots of love under the GPL by Jonathan
 * Blandford <jrb@gnome.org>
 */

#include <config.h>

#include <string.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <glade/glade.h>

#include "theme-common.h"
#include "capplet-util.h"
#include "activate-settings-daemon.h"
#include "gconf-property-editor.h"

#define GTK_THEME_KEY "/desktop/gnome/interface/gtk_theme"
#define GTK_FONT_KEY "/desktop/gnome/interface/font_name"
#define DESKTOP_FONT_NAME_KEY "/apps/nautilus/preferences/default_font"
#define DESKTOP_FONT_SIZE_KEY "/apps/nautilus/preferences/default_font_size"

#define MAX_ELEMENTS_BEFORE_SCROLLING 8

enum
{
  THEME_NAME_COLUMN,
  N_COLUMNS
};

gboolean setting_model = FALSE;


static GladeXML *
create_dialog (void)
{
  GladeXML *dialog;

  dialog = glade_xml_new (GLADEDIR "/gnome-font-and-theme-properties.glade", "font_and_theme_dialog", NULL);

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
theme_changed_func (gpointer data)
{
	g_print ("boo\n");
}

static void
setup_dialog (GladeXML *dialog)
{
  GConfClient *client;
  GtkWidget *widget;
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  GObject *peditor;

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
  gtk_tree_view_set_model (GTK_TREE_VIEW (WID ("theme_treeview")), model);
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (WID ("theme_treeview")));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
  g_signal_connect (G_OBJECT (selection), "changed", (GCallback) theme_selection_changed, NULL);

  read_themes (dialog);
  theme_common_register_theme_change (theme_changed_func, dialog);

  peditor = gconf_peditor_new_font (NULL, GTK_FONT_KEY,
		  		    WID ("application_font"),
				    PEDITOR_FONT_COMBINED, NULL);

  peditor = gconf_peditor_new_font (NULL, DESKTOP_FONT_NAME_KEY,
		  		    WID ("desktop_font"),
				    PEDITOR_FONT_NAME, NULL);

  peditor = gconf_peditor_new_font (NULL, DESKTOP_FONT_SIZE_KEY,
		  		    WID ("desktop_font"),
				    PEDITOR_FONT_SIZE, NULL);

  widget = WID ("font_and_theme_dialog");
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT (widget), "response", gtk_main_quit, NULL);
  g_signal_connect (G_OBJECT (widget), "close", gtk_main_quit, NULL);
}

int
main (int argc, char *argv[])
{
  GladeXML *dialog;

  gtk_init (&argc, &argv);

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
