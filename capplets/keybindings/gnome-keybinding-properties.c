/* This program was written with lots of love under the GPL by Jonathan
 * Blandford <jrb@gnome.org>
 */

#include <config.h>

#include <string.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>
#include <gdk/gdkx.h>
#include <glade/glade.h>
#include <X11/Xatom.h>

#include "gnome-theme-info.h"
#include "wm-common.h"
#include "capplet-util.h"
#include "eggcellrendererkeys.h"
#include "activate-settings-daemon.h"

#define LABEL_DATA "gnome-keybinding-properties-label"
#define KEY_THEME_KEY "/desktop/gnome/interface/gtk_key_theme"
#define MAX_ELEMENTS_BEFORE_SCROLLING 10

#if defined(__powerpc__) && defined (__linux__)
#define USE_FBLEVEL
#else
#undef USE_FBLEVEL
#endif

typedef enum {
  ALWAYS_VISIBLE,
  N_WORKSPACES_GT
} KeyListEntryVisibility;

typedef struct
{
  const char *name;
  KeyListEntryVisibility visibility;
  gint data;
} KeyListEntry;

const KeyListEntry desktop_key_list[] =
{
  { "/apps/gnome_settings_daemon/keybindings/help", ALWAYS_VISIBLE, 0 },
  { "/apps/gnome_settings_daemon/keybindings/power", ALWAYS_VISIBLE, 0 },
  { "/apps/gnome_settings_daemon/keybindings/sleep", ALWAYS_VISIBLE, 0 },
  { "/apps/gnome_settings_daemon/keybindings/screensaver", ALWAYS_VISIBLE, 0 },
  { "/apps/gnome_settings_daemon/keybindings/home", ALWAYS_VISIBLE, 0 },
  { "/apps/gnome_settings_daemon/keybindings/search", ALWAYS_VISIBLE, 0 },
  { "/apps/gnome_settings_daemon/keybindings/email", ALWAYS_VISIBLE, 0 },
  { "/apps/gnome_settings_daemon/keybindings/www", ALWAYS_VISIBLE, 0 },
  { "/apps/metacity/global_keybindings/panel_run_dialog", ALWAYS_VISIBLE, 0 },
  { "/apps/metacity/global_keybindings/panel_main_menu", ALWAYS_VISIBLE, 0 },
  { "/apps/metacity/global_keybindings/run_command_screenshot", ALWAYS_VISIBLE, 0 },
  { "/apps/metacity/global_keybindings/run_command_window_screenshot", ALWAYS_VISIBLE, 0 },
#ifdef USE_FBLEVEL
  { "/apps/gnome_settings_daemon/keybindings/brightness_down", ALWAYS_VISIBLE, 0 },
  { "/apps/gnome_settings_daemon/keybindings/brightness_up", ALWAYS_VISIBLE, 0 },
#endif
  { NULL }
};
const KeyListEntry sounds_key_list[] =
{
  { "/apps/gnome_settings_daemon/keybindings/volume_mute", ALWAYS_VISIBLE, 0 },
  { "/apps/gnome_settings_daemon/keybindings/volume_down", ALWAYS_VISIBLE, 0 },
  { "/apps/gnome_settings_daemon/keybindings/volume_up", ALWAYS_VISIBLE, 0 },
  /* Other ones that need keysyms bindings */
  { "/apps/gnome_settings_daemon/keybindings/play", ALWAYS_VISIBLE, 0 },
  { "/apps/gnome_settings_daemon/keybindings/pause", ALWAYS_VISIBLE, 0 },
  { "/apps/gnome_settings_daemon/keybindings/stop", ALWAYS_VISIBLE, 0 },
  { "/apps/gnome_settings_daemon/keybindings/previous", ALWAYS_VISIBLE, 0 },
  { "/apps/gnome_settings_daemon/keybindings/next", ALWAYS_VISIBLE, 0 },
  { "/apps/gnome_settings_daemon/keybindings/eject", ALWAYS_VISIBLE, 0 },
  { NULL }
};

const KeyListEntry metacity_key_list[] =
{
  { "/apps/metacity/window_keybindings/activate_window_menu",      ALWAYS_VISIBLE,  0 },
  { "/apps/metacity/window_keybindings/toggle_fullscreen",         ALWAYS_VISIBLE,  0 },
  { "/apps/metacity/window_keybindings/toggle_maximized",          ALWAYS_VISIBLE,  0 },
  { "/apps/metacity/window_keybindings/maximize",                  ALWAYS_VISIBLE,  0 },
  { "/apps/metacity/window_keybindings/unmaximize",                ALWAYS_VISIBLE,  0 },
  { "/apps/metacity/window_keybindings/toggle_shaded",             ALWAYS_VISIBLE,  0 },
  { "/apps/metacity/window_keybindings/close",                     ALWAYS_VISIBLE,  0 },
  { "/apps/metacity/window_keybindings/minimize",                  ALWAYS_VISIBLE,  0 },
  { "/apps/metacity/window_keybindings/begin_move",                ALWAYS_VISIBLE,  0 },
  { "/apps/metacity/window_keybindings/begin_resize",              ALWAYS_VISIBLE,  0 },
  { "/apps/metacity/window_keybindings/toggle_on_all_workspaces",  N_WORKSPACES_GT, 1 },
  /* These next three are out of order compared to the schemas file. */
  { "/apps/metacity/window_keybindings/raise_or_lower",            ALWAYS_VISIBLE,  0 },
  { "/apps/metacity/window_keybindings/raise",                     ALWAYS_VISIBLE,  0 },
  { "/apps/metacity/window_keybindings/lower",                     ALWAYS_VISIBLE,  0 },
  { "/apps/metacity/window_keybindings/maximize_vertically",       ALWAYS_VISIBLE,  0 },
  { "/apps/metacity/window_keybindings/maximize_horizontally",     ALWAYS_VISIBLE,  0 },
  { "/apps/metacity/window_keybindings/move_to_workspace_1",       N_WORKSPACES_GT, 1 },
  { "/apps/metacity/window_keybindings/move_to_workspace_2",       N_WORKSPACES_GT, 1 },
  { "/apps/metacity/window_keybindings/move_to_workspace_3",       N_WORKSPACES_GT, 2 },
  { "/apps/metacity/window_keybindings/move_to_workspace_4",       N_WORKSPACES_GT, 3 },
  { "/apps/metacity/window_keybindings/move_to_workspace_5",       N_WORKSPACES_GT, 4 },
  { "/apps/metacity/window_keybindings/move_to_workspace_6",       N_WORKSPACES_GT, 5 },
  { "/apps/metacity/window_keybindings/move_to_workspace_7",       N_WORKSPACES_GT, 6 },
  { "/apps/metacity/window_keybindings/move_to_workspace_8",       N_WORKSPACES_GT, 7 },
  { "/apps/metacity/window_keybindings/move_to_workspace_9",       N_WORKSPACES_GT, 8 },
  { "/apps/metacity/window_keybindings/move_to_workspace_10",      N_WORKSPACES_GT, 9 },
  { "/apps/metacity/window_keybindings/move_to_workspace_11",      N_WORKSPACES_GT, 10 },
  { "/apps/metacity/window_keybindings/move_to_workspace_12",      N_WORKSPACES_GT, 11 },
  { "/apps/metacity/window_keybindings/move_to_workspace_left",    N_WORKSPACES_GT, 1 },
  { "/apps/metacity/window_keybindings/move_to_workspace_right",   N_WORKSPACES_GT, 1 },
  { "/apps/metacity/window_keybindings/move_to_workspace_up",      N_WORKSPACES_GT, 1 },
  { "/apps/metacity/window_keybindings/move_to_workspace_down",    N_WORKSPACES_GT, 1 },
  { "/apps/metacity/global_keybindings/switch_windows",            ALWAYS_VISIBLE,  0 },
  { "/apps/metacity/global_keybindings/switch_panels",             ALWAYS_VISIBLE,  0 },
  { "/apps/metacity/global_keybindings/cycle_windows",             ALWAYS_VISIBLE,  0 },
  { "/apps/metacity/global_keybindings/cycle_panels",              ALWAYS_VISIBLE,  0 },
  { "/apps/metacity/global_keybindings/show_desktop",              ALWAYS_VISIBLE,  0 },
  { "/apps/metacity/global_keybindings/switch_to_workspace_1",     N_WORKSPACES_GT, 1 },
  { "/apps/metacity/global_keybindings/switch_to_workspace_2",     N_WORKSPACES_GT, 1 },
  { "/apps/metacity/global_keybindings/switch_to_workspace_3",     N_WORKSPACES_GT, 2 },
  { "/apps/metacity/global_keybindings/switch_to_workspace_4",     N_WORKSPACES_GT, 3 },
  { "/apps/metacity/global_keybindings/switch_to_workspace_5",     N_WORKSPACES_GT, 4 },
  { "/apps/metacity/global_keybindings/switch_to_workspace_6",     N_WORKSPACES_GT, 5 },
  { "/apps/metacity/global_keybindings/switch_to_workspace_7",     N_WORKSPACES_GT, 6 },
  { "/apps/metacity/global_keybindings/switch_to_workspace_8",     N_WORKSPACES_GT, 7 },
  { "/apps/metacity/global_keybindings/switch_to_workspace_9",     N_WORKSPACES_GT, 8 },
  { "/apps/metacity/global_keybindings/switch_to_workspace_10",    N_WORKSPACES_GT, 9 },
  { "/apps/metacity/global_keybindings/switch_to_workspace_11",    N_WORKSPACES_GT, 10 },
  { "/apps/metacity/global_keybindings/switch_to_workspace_12",    N_WORKSPACES_GT, 11 },
  { "/apps/metacity/global_keybindings/switch_to_workspace_left",  N_WORKSPACES_GT, 1 },
  { "/apps/metacity/global_keybindings/switch_to_workspace_right", N_WORKSPACES_GT, 1 },
  { "/apps/metacity/global_keybindings/switch_to_workspace_up",    N_WORKSPACES_GT, 1 },
  { "/apps/metacity/global_keybindings/switch_to_workspace_down",  N_WORKSPACES_GT, 1 },
  { NULL }
};

enum
{
  DESCRIPTION_COLUMN,
  KEYENTRY_COLUMN,
  N_COLUMNS
};

typedef struct
{
  char *gconf_key;
  guint keyval;
  guint keycode;
  EggVirtualModifierType mask;
  gboolean editable;
  GtkTreeModel *model;
  guint gconf_cnxn;
  char *description;
} KeyEntry;

static void  reload_key_entries (gpointer                wm_name,
                                 GladeXML               *dialog);
static char* binding_name       (guint                   keyval,
				 guint			 keycode,
                                 EggVirtualModifierType  mask,
                                 gboolean                translate);

static GtkTreeModel*
get_real_model (GtkTreeView *tree_view)
{
  GtkTreeModel *model;
  GtkTreeModel *submodel;
  
  model = gtk_tree_view_get_model (tree_view);

  if (model)
    submodel = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (model));
  else
    submodel = NULL;
  
  return submodel;
}

static void
menu_item_activate (GtkWidget *menu_item,
		    gpointer   unused)
{
  gchar *key_theme;
  gchar *current_key_theme;
  GConfClient *client;
  GError *error = NULL;

  client = gconf_client_get_default ();

  key_theme = g_object_get_data (G_OBJECT (menu_item), LABEL_DATA);
  g_return_if_fail (key_theme != NULL);

  current_key_theme = gconf_client_get_string (client, KEY_THEME_KEY, &error);
  if (current_key_theme && strcmp (current_key_theme, key_theme))
    {
      gconf_client_set_string (client, KEY_THEME_KEY, key_theme, NULL);
    }
}

static GtkWidget *
make_key_theme_menu_item (const gchar *key_theme)
{
  GtkWidget *retval;

  if (!strcmp (key_theme, "Default"))
    {
      retval = gtk_menu_item_new_with_label (_("GNOME Default"));
      g_object_set_data_full (G_OBJECT (retval), LABEL_DATA, g_strdup ("Default"), g_free);
    }
  else
    {
      retval = gtk_menu_item_new_with_label (key_theme);
      g_object_set_data_full (G_OBJECT (retval), LABEL_DATA, g_strdup (key_theme), g_free);
    }
  g_signal_connect (G_OBJECT (retval), "activate", G_CALLBACK (menu_item_activate), NULL);
  gtk_widget_show (retval);

  return retval;
}

static GladeXML *
create_dialog (void)
{
  GladeXML *dialog;

  dialog = glade_xml_new (GNOMECC_DATA_DIR "/interfaces/gnome-keybinding-properties.glade", "gnome-keybinding-dialog", NULL);

  return dialog;
}

static char*
binding_name (guint                   keyval,
	      guint		      keycode,
              EggVirtualModifierType  mask,
              gboolean                translate)
{
  if (keyval != 0 || keycode != 0)
    return egg_virtual_accelerator_name (keyval, keycode, mask);
  else
    return translate ? g_strdup (_("Disabled")) : g_strdup ("disabled");
}

static gboolean
binding_from_string (const char             *str,
                     guint                  *accelerator_key,
		     guint		    *keycode,
                     EggVirtualModifierType *accelerator_mods)
{
  g_return_val_if_fail (accelerator_key != NULL, FALSE);
  
  if (str == NULL || (str && strcmp (str, "disabled") == 0))
    {
      *accelerator_key = 0;
      *keycode = 0;
      *accelerator_mods = 0;
      return TRUE;
    }

  egg_accelerator_parse_virtual (str, accelerator_key, keycode, accelerator_mods);
  
  if (*accelerator_key == 0)
    return FALSE;
  else
    return TRUE;
}

static void
accel_set_func (GtkTreeViewColumn *tree_column,
                GtkCellRenderer   *cell,
                GtkTreeModel      *model,
                GtkTreeIter       *iter,
                gpointer           data)
{
  KeyEntry *key_entry;
  
  gtk_tree_model_get (model, iter,
                      KEYENTRY_COLUMN, &key_entry,
                      -1);

  if (key_entry == NULL)
    g_object_set (G_OBJECT (cell),
		  "visible", FALSE,
		  NULL);
  else if (! key_entry->editable)
    g_object_set (G_OBJECT (cell),
		  "visible", TRUE,
		  "editable", FALSE,
		  "accel_key", key_entry->keyval,
		  "accel_mask", key_entry->mask,
		  "keycode", key_entry->keycode,
		  "style", PANGO_STYLE_ITALIC,
		  NULL);
  else
    g_object_set (G_OBJECT (cell),
		  "visible", TRUE,
		  "editable", TRUE,
		  "accel_key", key_entry->keyval,
		  "accel_mask", key_entry->mask,
		  "keycode", key_entry->keycode,
		  "style", PANGO_STYLE_NORMAL,
		  NULL);
}

static gboolean
keybinding_key_changed_foreach (GtkTreeModel *model,
				GtkTreePath  *path,
				GtkTreeIter  *iter,
				gpointer      user_data)
{
  KeyEntry *key_entry;
  KeyEntry *tmp_key_entry;

  key_entry = (KeyEntry *)user_data;
  gtk_tree_model_get (key_entry->model, iter,
		      KEYENTRY_COLUMN, &tmp_key_entry,
		      -1);

  if (key_entry == tmp_key_entry)
    {
      gtk_tree_model_row_changed (key_entry->model, path, iter);
      return TRUE;
    }
  return FALSE;
}

static void
keybinding_key_changed (GConfClient *client,
			guint        cnxn_id,
			GConfEntry  *entry,
			gpointer     user_data)
{
  KeyEntry *key_entry;
  const gchar *key_value;

  key_entry = (KeyEntry *)user_data;
  key_value = gconf_value_get_string (entry->value);

  binding_from_string (key_value, &key_entry->keyval, &key_entry->keycode, &key_entry->mask);
  key_entry->editable = gconf_entry_get_is_writable (entry);

  /* update the model */
  gtk_tree_model_foreach (key_entry->model, keybinding_key_changed_foreach, key_entry);
}

static int
keyentry_sort_func (GtkTreeModel *model,
                    GtkTreeIter  *a,
                    GtkTreeIter  *b,
                    gpointer      user_data)
{
  KeyEntry *key_entry_a;
  KeyEntry *key_entry_b;
  char *name_a;
  char *name_b;
  int retval;

  /* mmmmm, super-slow. */

  key_entry_a = NULL;
  gtk_tree_model_get (model, a,
                      KEYENTRY_COLUMN, &key_entry_a,
                      -1);  

  key_entry_b = NULL;
  gtk_tree_model_get (model, b,
                      KEYENTRY_COLUMN, &key_entry_b,
                      -1);

  
  if (key_entry_a != NULL)
    name_a = binding_name (key_entry_a->keyval,
		    	   key_entry_a->keycode,
                           key_entry_a->mask,
                           TRUE);
  else
    name_a = NULL;

  if (key_entry_b != NULL)
    name_b = binding_name (key_entry_b->keyval,
		    	   key_entry_b->keycode,
                           key_entry_b->mask,
                           TRUE);
  else
    name_b = NULL;

  if (name_a && name_b)
    retval = g_utf8_collate (name_a, name_b);
  else if (name_a)
    retval = 1;
  else if (name_b)
    retval = -1;
  else
    retval = 0;

  g_free (name_a);
  g_free (name_b);

  return retval;
}

static void
clear_old_model (GladeXML  *dialog,
		 GtkWidget *tree_view)
{
  GtkTreeModel *model;
  GtkTreeModel *sort_model;
  GtkTreeIter iter;
  KeyEntry *key_entry;
  gboolean valid;
  GConfClient *client;

  client = gconf_client_get_default ();
  model = get_real_model (GTK_TREE_VIEW (tree_view));

  if (model != NULL)
    {
      g_object_ref (model);

      for (valid = gtk_tree_model_get_iter_first (model, &iter);
	   valid;
	   valid = gtk_tree_model_iter_next (model, &iter))
	{
	  gtk_tree_model_get (model, &iter,
			      KEYENTRY_COLUMN, &key_entry,
			      -1);
	  if (key_entry != NULL)
	    {
	      gconf_client_notify_remove (client, key_entry->gconf_cnxn);
	      g_free (key_entry->gconf_key);
              g_free (key_entry->description);
	      g_free (key_entry);
            }
	}
      g_object_unref (model);
    }

  model = (GtkTreeModel *) gtk_tree_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER);
  
  sort_model = gtk_tree_model_sort_new_with_model (model);

  /* N_COLUMNS is just a place to stick the extra sort function */
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (sort_model),
                                   N_COLUMNS,
                                   keyentry_sort_func,
                                   NULL, NULL);
  
  gtk_tree_view_set_model (GTK_TREE_VIEW (tree_view), sort_model);

  g_object_unref (G_OBJECT (model));
  g_object_unref (G_OBJECT (sort_model));
  
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (WID ("actions_swindow")),
				  GTK_POLICY_NEVER, GTK_POLICY_NEVER);
  gtk_widget_set_usize (WID ("actions_swindow"), -1, -1);
}

static gboolean
count_rows_foreach (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
  gint *rows = data;

  (*rows)++;

  return FALSE;
}

static gboolean
should_show_key (const KeyListEntry *entry)
{
  gint workspaces;
  
  switch (entry->visibility) {
  case ALWAYS_VISIBLE:
    return TRUE;
  case N_WORKSPACES_GT:
    workspaces = gconf_client_get_int (gconf_client_get_default (),
				       "/apps/metacity/general/num_workspaces", NULL);
    if (workspaces > entry->data)
      return TRUE;
    else
      return FALSE;
    break;
  }

  return FALSE;
}

static void
append_keys_to_tree (GladeXML           *dialog,
		     const gchar        *title,
		     const KeyListEntry *keys_list)
{
  GConfClient *client;
  GtkTreeIter parent_iter;
  GtkTreeModel *model;
  gint i, j;

  client = gconf_client_get_default ();
  model = get_real_model (GTK_TREE_VIEW (WID ("shortcut_treeview")));

  i = 0;
  gtk_tree_model_foreach (model, count_rows_foreach, &i);

  gtk_tree_store_append (GTK_TREE_STORE (model), &parent_iter, NULL);
  gtk_tree_store_set (GTK_TREE_STORE (model), &parent_iter,
  		      DESCRIPTION_COLUMN, title,
  		      -1);

  for (j = 0; keys_list[j].name != NULL; j++)
    {
      GConfEntry *entry;
      GConfSchema *schema = NULL;
      KeyEntry *key_entry;
      GError *error = NULL;
      GtkTreeIter iter;
      const gchar *key_string;
      gchar *key_value;

      if (!should_show_key (&keys_list[j]))
	continue;
      
      key_string = keys_list[j].name;

      entry = gconf_client_get_entry (client,
                                      key_string,
				      NULL,
				      TRUE,
				      &error);
      if (error || entry == NULL)
	{
	  /* We don't actually want to popup a dialog - just skip this one */
	  if (error)
	    g_error_free (error);
	  continue;
	}

      if (gconf_entry_get_schema_name (entry))
	schema = gconf_client_get_schema (client, gconf_entry_get_schema_name (entry), &error);
      
      if (error || schema == NULL)
	{
	  /* We don't actually want to popup a dialog - just skip this one */
	  if (error)
	    g_error_free (error);
	  continue;
	}

      key_value = gconf_client_get_string (client, key_string, &error);

      key_entry = g_new0 (KeyEntry, 1);
      key_entry->gconf_key = g_strdup (key_string);
      key_entry->editable = gconf_entry_get_is_writable (entry);
      key_entry->model = model;
      gconf_client_add_dir (client, key_string, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
      key_entry->gconf_cnxn = gconf_client_notify_add (client,
						       key_string,
						       (GConfClientNotifyFunc) &keybinding_key_changed,
						       key_entry, NULL, NULL);
      binding_from_string (key_value, &key_entry->keyval, &key_entry->keycode, &key_entry->mask);
      g_free (key_value);
      key_entry->description = g_strdup (gconf_schema_get_short_desc (schema));

      if (i == MAX_ELEMENTS_BEFORE_SCROLLING)
	{
	  GtkRequisition rectangle;
	  gtk_widget_ensure_style (WID ("shortcut_treeview"));
	  gtk_widget_size_request (WID ("shortcut_treeview"), &rectangle);
	  gtk_widget_set_size_request (WID ("shortcut_treeview"), -1, rectangle.height);
	  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (WID ("actions_swindow")),
					  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	}
      i++;
      gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &parent_iter);
      if (gconf_schema_get_short_desc (schema))
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
			    DESCRIPTION_COLUMN,
                            key_entry->description,
			    KEYENTRY_COLUMN, key_entry,
			    -1);
      else
	gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
			    DESCRIPTION_COLUMN, _("<Unknown Action>"),
			    KEYENTRY_COLUMN, key_entry,
			    -1);
      gtk_tree_view_expand_all (GTK_TREE_VIEW (WID ("shortcut_treeview")));
      gconf_entry_free (entry);
      gconf_schema_free (schema);
    }

  if (i == 0)
      gtk_widget_hide (WID ("shortcuts_vbox"));
  else
      gtk_widget_show (WID ("shortcuts_vbox"));
}

static void
reload_key_entries (gpointer wm_name, GladeXML *dialog)
{
  clear_old_model (dialog, WID ("shortcut_treeview"));
  
  append_keys_to_tree (dialog, _("Desktop"), desktop_key_list);
  append_keys_to_tree (dialog, _("Sound"), sounds_key_list);
  
  if (strcmp((char *) wm_name, WM_COMMON_METACITY) == 0)
    {
      append_keys_to_tree (dialog, _("Window Management"), metacity_key_list);
    }
}

static void
key_entry_controlling_key_changed (GConfClient *client,
				   guint        cnxn_id,
				   GConfEntry  *entry,
				   gpointer     user_data)
{
  reload_key_entries (wm_common_get_current_window_manager(), user_data);
}

static void
key_theme_changed (GConfClient *client,
		   guint        cnxn_id,
		   GConfEntry  *entry,
		   gpointer     user_data)
{
  GtkWidget *omenu = (GtkWidget *) user_data;
  GtkWidget *menu;
  GtkWidget *menu_item;
  GConfValue *value;
  const gchar *new_key_theme;
  GList *list;
  gint i = 0;

  menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (omenu));
  value = gconf_entry_get_value (entry);

  g_return_if_fail (value != NULL);

  new_key_theme = gconf_value_get_string (value);

  for (list = GTK_MENU_SHELL (menu)->children; list; list = list->next, i++)
    {
      gchar *text;

      menu_item = GTK_WIDGET (list->data);
      text = g_object_get_data (G_OBJECT (menu_item), LABEL_DATA);
      if (! strcmp (text, new_key_theme))
	{
	  if (gtk_option_menu_get_history (GTK_OPTION_MENU (omenu)) != i)
	    gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), i);
	  return;
	}
    }

  /* We didn't find our theme.  Add it to our list. */
  menu_item = make_key_theme_menu_item (new_key_theme);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);  
  gtk_option_menu_set_history (GTK_OPTION_MENU (omenu), i);
}


static gboolean
cb_check_for_uniqueness (GtkTreeModel *model,
			 GtkTreePath  *path,
			 GtkTreeIter  *iter,
			 KeyEntry *new_key)
{
  KeyEntry *element;

  gtk_tree_model_get (new_key->model, iter,
		      KEYENTRY_COLUMN, &element,
		      -1);

  /* no conflict for : blanks, different modifiers, or ourselves */
  if (element == NULL || new_key->mask != element->mask ||
      !strcmp (new_key->gconf_key, element->gconf_key))
    return FALSE;

  if (new_key->keyval != 0) {
      if (new_key->keyval != element->keyval)
	  return FALSE;
  } else if (element->keyval != 0 || new_key->keycode != element->keycode)
    return FALSE;

  new_key->editable = FALSE;
  new_key->gconf_key = element->gconf_key;
  new_key->description = element->description;
  return TRUE;
}

static void
accel_edited_callback (GtkCellRendererText   *cell,
                       const char            *path_string,
                       guint                  keyval,
                       EggVirtualModifierType mask,
		       guint		      keycode,
                       gpointer               data)
{
  GtkTreeView *view = (GtkTreeView *)data;
  GtkTreeModel *model;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;
  KeyEntry *key_entry, tmp_key;
  GError *err = NULL;
  char *str;
  guint32 c;

  model = gtk_tree_view_get_model (view);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);
  gtk_tree_model_get (model, &iter,
		      KEYENTRY_COLUMN, &key_entry,
		      -1);

  /* sanity check */
  if (key_entry == NULL)
    return;

  model = get_real_model (view);
  tmp_key.model  = model;
  tmp_key.keyval = keyval;
  tmp_key.keycode = keycode;
  tmp_key.mask   = mask;
  tmp_key.gconf_key = key_entry->gconf_key;
  tmp_key.description = NULL;
  tmp_key.editable = TRUE; /* kludge to stuff in a return flag */

  if (keyval != 0 || keycode != 0) /* any number of keys can be disabled */
    gtk_tree_model_foreach (model,
      (GtkTreeModelForeachFunc) cb_check_for_uniqueness,
      &tmp_key);

  /* flag to see if the new accelerator was in use by something */
  if (!tmp_key.editable)
    {
      GtkWidget *dialog;
      char *name;

      name = egg_virtual_accelerator_name (keyval, keycode, mask);
      
      dialog =
        gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
                                GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                GTK_MESSAGE_WARNING,
                                GTK_BUTTONS_CANCEL,
                                _("The shortcut \"%s\" is already used for:\n \"%s\"\n"),
                                name,
                                tmp_key.description ?
                                tmp_key.description : tmp_key.gconf_key);
      g_free (name);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      /* set it back to its previous value. */
      egg_cell_renderer_keys_set_accelerator (EGG_CELL_RENDERER_KEYS (cell),
					      key_entry->keyval, key_entry->keycode, key_entry->mask);
      return;
    }

  /* see if a user really wants to lose an alphanumeric key to a binding */
  if (keyval != 0 && 
      (mask == 0 || mask == EGG_VIRTUAL_SHIFT_MASK) && /* only unmodified */
       (c = gdk_keyval_to_unicode (keyval)) != 0)
    {
      char *name = egg_virtual_accelerator_name (keyval, keycode, mask);
      GtkWidget *dialog =
        gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
                                GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                GTK_MESSAGE_WARNING,
				GTK_BUTTONS_YES_NO,
                                _("The '%s' key types something.  Are you sure you want to lose it to a keybinding ?"),
                                name);
      g_free (name);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      /* set it back to its previous value. */
      egg_cell_renderer_keys_set_accelerator (EGG_CELL_RENDERER_KEYS (cell),
					      key_entry->keyval, key_entry->keycode, key_entry->mask);
      return;
  }

  str = binding_name (keyval, keycode, mask, FALSE);

  gconf_client_set_string (gconf_client_get_default(),
                           key_entry->gconf_key,
                           str,
                           &err);
  g_free (str);
  
  if (err != NULL)
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
				       GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
				       GTK_MESSAGE_WARNING,
				       GTK_BUTTONS_OK,
				       _("Error setting new accelerator in configuration database: %s\n"),
				       err->message);
      gtk_dialog_run (GTK_DIALOG (dialog));

      gtk_widget_destroy (dialog);
      g_error_free (err);
      key_entry->editable = FALSE;
    }
}

static void
accel_cleared_callback (GtkCellRendererText *cell,
			const char          *path_string,
			gpointer             data)
{
  GtkTreeView *view = (GtkTreeView *) data;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  KeyEntry *key_entry;
  GtkTreeIter iter;
  GError *err = NULL;
  GtkTreeModel *model;

  model = gtk_tree_view_get_model (view);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);
  gtk_tree_model_get (model, &iter,
		      KEYENTRY_COLUMN, &key_entry,
		      -1);

  /* sanity check */
  if (key_entry == NULL)
    return;

  /* Unset the key */
  gconf_client_set_string (gconf_client_get_default(),
			   key_entry->gconf_key,
			   "disabled",
			   &err);
  if (err != NULL)
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
				       GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
				       GTK_MESSAGE_WARNING,
				       GTK_BUTTONS_OK,
				       _("Error unsetting accelerator in configuration database: %s\n"),
				       err->message);
      gtk_dialog_run (GTK_DIALOG (dialog));

      gtk_widget_destroy (dialog);
      g_error_free (err);
      key_entry->editable = FALSE;
    }
}

static void
theme_changed_func (gpointer  uri,
		    GladeXML *dialog)
{
  GConfClient *client;
  GtkWidget *omenu;
  GtkWidget *menu;
  GtkWidget *menu_item;
  GConfEntry *entry;
  GList *key_theme_list;
  GList *list;

  client = gconf_client_get_default ();
  key_theme_list = gnome_theme_info_find_by_type (GNOME_THEME_GTK_2_KEYBINDING);

  omenu = WID ("key_theme_omenu");
  menu = gtk_menu_new ();
  for (list = key_theme_list; list; list = list->next)
    {
      GnomeThemeInfo *info = list->data;

      if (! info->has_keybinding)
	continue;

      menu_item = make_key_theme_menu_item (info->name);
      if (!strcmp (info->name, "Default"))
      /* Put default first, always */
        gtk_menu_shell_prepend (GTK_MENU_SHELL (menu), menu_item);
      else
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_item);
    }

  gtk_widget_show (menu);
  gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);

  /* Initialize the option menu */
  entry = gconf_client_get_entry (client,
				  KEY_THEME_KEY,
				  NULL, TRUE, NULL);

  key_theme_changed (client, 0, entry, omenu);
}


typedef struct
{
  GtkTreeView *tree_view;
  GtkTreePath *path;
} IdleData;

static gboolean
real_start_editing_cb (IdleData *idle_data)
{
  gtk_widget_grab_focus (GTK_WIDGET (idle_data->tree_view));
  gtk_tree_view_set_cursor (idle_data->tree_view,
			    idle_data->path,
			    gtk_tree_view_get_column (idle_data->tree_view, 1),
			    TRUE);

  gtk_tree_path_free (idle_data->path);
  g_free (idle_data);
  return FALSE;
}

static gboolean
start_editing_cb (GtkTreeView    *tree_view,
		  GdkEventButton *event,
		  GladeXML       *dialog)
{
  GtkTreePath *path;

  if (event->window != gtk_tree_view_get_bin_window (tree_view))
    return FALSE;

  if (gtk_tree_view_get_path_at_pos (tree_view,
				     (gint) event->x,
				     (gint) event->y,
				     &path, NULL,
				     NULL, NULL))
    {
      IdleData *idle_data;

      if (gtk_tree_path_get_depth (path) == 1)
	{
	  gtk_tree_path_free (path);
	  return FALSE;
	}

      idle_data = g_new (IdleData, 1);
      idle_data->tree_view = tree_view;
      idle_data->path = path;
      g_signal_stop_emission_by_name (G_OBJECT (tree_view), "button_press_event");
      g_idle_add ((GSourceFunc) real_start_editing_cb, idle_data);
    }
  return TRUE;
}

static void
cb_dialog_response (GtkWidget *widget, gint response_id, gpointer data)
{
	if (response_id == GTK_RESPONSE_HELP)
		capplet_help (GTK_WINDOW (widget),
			      "user-guide.xml",
			      "goscustdesk-39");
	else
		gtk_main_quit ();
}

static void
setup_dialog (GladeXML *dialog)
{
  GConfClient *client;
  GList *key_theme_list;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkWidget *widget;
  gboolean found_keys = FALSE;
  GList *list;

  client = gconf_client_get_default ();

  key_theme_list = gnome_theme_info_find_by_type (GNOME_THEME_GTK_2_KEYBINDING);

  for (list = key_theme_list; list; list = list->next)
    {
      GnomeThemeInfo *info = list->data;
      if (info->has_keybinding)
	{
	  found_keys = TRUE;
	  break;
	}

    }
  if (! found_keys)
    {
      GtkWidget *msg_dialog = gtk_message_dialog_new (NULL, 0,
						      GTK_MESSAGE_ERROR,
						      GTK_BUTTONS_OK,
						      _("Unable to find any keyboard themes.  This means your GTK+ "
							"installation has been incompletely installed."));
      gtk_dialog_run (GTK_DIALOG (msg_dialog));
      gtk_widget_destroy (msg_dialog);

    }
  else
    {
      theme_changed_func (NULL, dialog);
      gnome_theme_info_register_theme_change ((GFunc) theme_changed_func, dialog);
      gconf_client_add_dir (client, "/desktop/gnome/interface", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
      gconf_client_notify_add (client,
			       KEY_THEME_KEY,
			       (GConfClientNotifyFunc) &key_theme_changed,
			       WID ("key_theme_omenu"), NULL, NULL);
    }


  g_signal_connect (GTK_TREE_VIEW (WID ("shortcut_treeview")),
		    "button_press_event",
		    G_CALLBACK (start_editing_cb), dialog),
		    
  column = gtk_tree_view_column_new_with_attributes (_("Action"),
						     gtk_cell_renderer_text_new (),
						     "text", DESCRIPTION_COLUMN,
						     NULL);
  gtk_tree_view_column_set_resizable (column, FALSE);

  gtk_tree_view_append_column (GTK_TREE_VIEW (WID ("shortcut_treeview")), column);
  gtk_tree_view_column_set_sort_column_id (column, DESCRIPTION_COLUMN);  
  
  renderer = (GtkCellRenderer *) g_object_new (EGG_TYPE_CELL_RENDERER_KEYS,
					       "editable", TRUE,
					       "accel_mode", EGG_CELL_RENDERER_KEYS_MODE_X,
					       NULL);

  g_signal_connect (G_OBJECT (renderer),
		    "accel_edited",
                    G_CALLBACK (accel_edited_callback),
                    WID ("shortcut_treeview"));

  g_signal_connect (G_OBJECT (renderer),
		    "accel_cleared",
                    G_CALLBACK (accel_cleared_callback),
                    WID ("shortcut_treeview"));

  column = gtk_tree_view_column_new_with_attributes (_("Shortcut"), renderer, NULL);
  gtk_tree_view_column_set_cell_data_func (column, renderer, accel_set_func, NULL, NULL);
  gtk_tree_view_column_set_resizable (column, FALSE);

  gtk_tree_view_append_column (GTK_TREE_VIEW (WID ("shortcut_treeview")), column);
  /* N_COLUMNS is just a place to stick the extra sort function */
  gtk_tree_view_column_set_sort_column_id (column, N_COLUMNS); 
  
  gconf_client_add_dir (client, "/apps/gnome_keybinding_properties", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  gconf_client_add_dir (client, "/apps/metacity/general", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  gconf_client_notify_add (client,
			   "/apps/metacity/general/num_workspaces",
			   (GConfClientNotifyFunc) &key_entry_controlling_key_changed,
			   dialog, NULL, NULL);

  /* set up the dialog */
  reload_key_entries (wm_common_get_current_window_manager(), dialog);

  widget = WID ("gnome-keybinding-dialog");
  capplet_set_icon (widget, "keyboard-shortcut.png");
  gtk_widget_show (widget);

  g_signal_connect (G_OBJECT (widget), "response", G_CALLBACK(cb_dialog_response), NULL);
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

  gnome_program_init ("gnome-keybinding-properties", VERSION, LIBGNOMEUI_MODULE, argc, argv,
		      GNOME_PARAM_APP_DATADIR, GNOMECC_DATA_DIR,
		      NULL);

  gnome_theme_init (NULL);

  activate_settings_daemon ();

  dialog = create_dialog ();
  wm_common_register_window_manager_change ((GFunc)(reload_key_entries), dialog);
  setup_dialog (dialog);
  
  gtk_main ();

  return 0;
}
