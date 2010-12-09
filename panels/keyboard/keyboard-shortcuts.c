/*
 * Copyright (C) 2010 Intel, Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Thomas Wood <thomas.wood@intel.com>
 *          Rodrigo Moya <rodrigo@gnome.org>
 */

#include <glib/gi18n.h>
#include <gconf/gconf-client.h>
#include "eggcellrendererkeys.h"
#include "keyboard-shortcuts.h"
#include "wm-common.h"

#define GCONF_BINDING_DIR "/desktop/gnome/keybindings"
#define WID(builder, name) (GTK_WIDGET (gtk_builder_get_object (builder, name)))

typedef struct {
  char *name;
  /* The gettext package to use to translate the section title */
  char *package;
  /* Name of the window manager the keys would apply to */
  char *wm_name;
  /* an array of KeyListEntry */
  GArray *entries;
} KeyList;

typedef enum {
  COMPARISON_NONE = 0,
  COMPARISON_GT,
  COMPARISON_LT,
  COMPARISON_EQ
} Comparison;

typedef struct
{
  char *name;
  int value;
  char *key;
  char *description_name;
  char *cmd_name;
  Comparison comparison;
} KeyListEntry;

typedef struct
{
  char *gconf_key;
  guint keyval;
  guint keycode;
  EggVirtualModifierType mask;
  gboolean editable;
  GtkTreeModel *model;
  char *description;
  char *desc_gconf_key;
  gboolean desc_editable;
  char *command;
  char *cmd_gconf_key;
  gboolean cmd_editable;
  guint gconf_cnxn;
  guint gconf_cnxn_desc;
  guint gconf_cnxn_cmd;
} KeyEntry;

enum
{
  DESCRIPTION_COLUMN,
  KEYENTRY_COLUMN,
  N_COLUMNS
};

static guint maybe_block_accels_id = 0;
static GtkWidget *custom_shortcut_dialog = NULL;
static GtkWidget *custom_shortcut_name_entry = NULL;
static GtkWidget *custom_shortcut_command_entry = NULL;
static GHashTable *kb_sections = NULL;

static void
free_key_array (GPtrArray *keys)
{
  if (keys != NULL)
    {
      gint i;

      for (i = 0; i < keys->len; i++)
        {
	  KeyEntry *entry;

	  entry = g_ptr_array_index (keys, i);
	  g_free (entry->gconf_key);
	  g_free (entry->description);
	  g_free (entry->desc_gconf_key);
	  g_free (entry->command);
	  g_free (entry->cmd_gconf_key);
	}

      g_ptr_array_free (keys, TRUE);
    }
}

static gboolean
should_show_key (const KeyListEntry *entry)
{
  int value;
  GConfClient *client;

  if (entry->comparison == COMPARISON_NONE)
    return TRUE;

  g_return_val_if_fail (entry->key != NULL, FALSE);

  client = gconf_client_get_default();
  value = gconf_client_get_int (client, entry->key, NULL);
  g_object_unref (client);

  switch (entry->comparison) {
  case COMPARISON_NONE:
    /* For compiler warnings */
    g_assert_not_reached ();
    return FALSE;
  case COMPARISON_GT:
    if (value > entry->value)
      return TRUE;
    break;
  case COMPARISON_LT:
    if (value < entry->value)
      return TRUE;
    break;
  case COMPARISON_EQ:
    if (value == entry->value)
      return TRUE;
    break;
  }

  return FALSE;
}

static gboolean
binding_from_string (const char             *str,
                     guint                  *accelerator_key,
		     guint		    *keycode,
                     EggVirtualModifierType *accelerator_mods)
{
  g_return_val_if_fail (accelerator_key != NULL, FALSE);

  if (str == NULL || strcmp (str, "disabled") == 0)
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
append_section (GtkBuilder *builder, const gchar *title, const KeyListEntry *keys_list)
{
  GPtrArray *keys_array;
  GtkTreeModel *model;
  GConfClient *client;
  GtkTreeIter iter;
  gint i;

  client = gconf_client_get_default ();
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (gtk_builder_get_object (builder, "section_treeview")));

  /* Add all KeyEntry's for this section */
  keys_array = g_ptr_array_new ();

  for (i = 0; keys_list[i].name != NULL; i++)
    {
      GConfEntry *entry;
      const gchar *key_string;
      gchar *description, *command, *key_value;
      KeyEntry *key_entry;

      if (!should_show_key (&keys_list[i]))
	continue;

      key_string = keys_list[i].name;

      entry = gconf_client_get_entry (client,
                                      key_string,
				      NULL,
				      TRUE,
				      NULL);
      if (entry == NULL)
	{
	  /* We don't actually want to popup a dialog - just skip this one */
	  continue;
	}

      if (keys_list[i].description_name != NULL)
        {
          description = gconf_client_get_string (client, keys_list[i].description_name, NULL);
        }
      else
        {
          description = NULL;

          if (gconf_entry_get_schema_name (entry))
            {
              GConfSchema *schema;

              schema = gconf_client_get_schema (client,
                                                gconf_entry_get_schema_name (entry),
                                                NULL);
              if (schema != NULL)
                {
                  description = g_strdup (gconf_schema_get_short_desc (schema));
                  gconf_schema_free (schema);
                }
            }
        }

      if (description == NULL)
        {
	  /* Only print a warning for keys that should have a schema */
	  if (keys_list[i].description_name == NULL)
	    g_warning ("No description for key '%s'", key_string);
	}

      if (keys_list[i].cmd_name != NULL)
        {
          command = gconf_client_get_string (client, keys_list[i].cmd_name, NULL);
        }
      else
        {
          command = NULL;
        }

      key_entry = g_new0 (KeyEntry, 1);
      key_entry->gconf_key = g_strdup (key_string);
      key_entry->editable = gconf_entry_get_is_writable (entry);
      key_entry->model = model;
      key_entry->description = description;
      key_entry->command = command;
      if (keys_list[i].description_name != NULL)
        {
          key_entry->desc_gconf_key =  g_strdup (keys_list[i].description_name);
          key_entry->desc_editable = gconf_client_key_is_writable (client, key_entry->desc_gconf_key, NULL);
          /* key_entry->gconf_cnxn_desc = gconf_client_notify_add (client, */
	  /* 							key_entry->desc_gconf_key, */
	  /* 							(GConfClientNotifyFunc) &keybinding_description_changed, */
	  /* 							key_entry, NULL, NULL); */
        }
      if (keys_list[i].cmd_name != NULL)
        {
          key_entry->cmd_gconf_key =  g_strdup (keys_list[i].cmd_name);
          key_entry->cmd_editable = gconf_client_key_is_writable (client, key_entry->cmd_gconf_key, NULL);
          /* key_entry->gconf_cnxn_cmd = gconf_client_notify_add (client, */
	  /* 						       key_entry->cmd_gconf_key, */
	  /* 							(GConfClientNotifyFunc) &keybinding_command_changed, */
	  /* 							key_entry, NULL, NULL); */
        }

      gconf_client_add_dir (client, key_string, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
      /* key_entry->gconf_cnxn = gconf_client_notify_add (client, */
      /* 						       key_string, */
      /* 						       (GConfClientNotifyFunc) &keybinding_key_changed, */
      /* 						       key_entry, NULL, NULL); */

      key_value = gconf_client_get_string (client, key_string, NULL);
      binding_from_string (key_value, &key_entry->keyval, &key_entry->keycode, &key_entry->mask);
      g_free (key_value);

      gconf_entry_free (entry);

      g_print ("Adding %s to section %s\n", key_entry->description, title);

      g_ptr_array_add (keys_array, key_entry);
    }

  g_object_unref (client);

  /* Add the keys to the hash table */
  if (keys_array->len > 0)
    {
      g_hash_table_insert (kb_sections, g_strdup (title), keys_array);

      /* Append the section to the left tree view */
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
			  DESCRIPTION_COLUMN, title,
			  -1);
    }
}

static void
parse_start_tag (GMarkupParseContext *ctx,
		 const gchar         *element_name,
		 const gchar        **attr_names,
		 const gchar        **attr_values,
		 gpointer             user_data,
		 GError             **error)
{
  KeyList *keylist = (KeyList *) user_data;
  KeyListEntry key;
  const char *name, *gconf_key;
  int value;
  Comparison comparison;

  name = NULL;

  /* The top-level element, names the section in the tree */
  if (g_str_equal (element_name, "KeyListEntries"))
    {
      const char *wm_name = NULL;
      const char *package = NULL;

      while (*attr_names && *attr_values)
        {
	  if (g_str_equal (*attr_names, "name"))
	    {
	      if (**attr_values)
		name = *attr_values;
	    } else if (g_str_equal (*attr_names, "wm_name")) {
	      if (**attr_values)
		wm_name = *attr_values;
	    } else if (g_str_equal (*attr_names, "package")) {
	      if (**attr_values)
		package = *attr_values;
	    }
	  ++attr_names;
	  ++attr_values;
	}

      if (name)
        {
	  if (keylist->name)
	    g_warning ("Duplicate section name");
	  g_free (keylist->name);
	  keylist->name = g_strdup (name);
	}
      if (wm_name)
        {
	  if (keylist->wm_name)
	    g_warning ("Duplicate window manager name");
	  g_free (keylist->wm_name);
	  keylist->wm_name = g_strdup (wm_name);
	}
      if (package)
        {
	  if (keylist->package)
	    g_warning ("Duplicate gettext package name");
	  g_free (keylist->package);
	  keylist->package = g_strdup (package);
	}
      return;
    }

  if (!g_str_equal (element_name, "KeyListEntry")
      || attr_names == NULL
      || attr_values == NULL)
    return;

  value = 0;
  comparison = COMPARISON_NONE;
  gconf_key = NULL;

  while (*attr_names && *attr_values)
    {
      if (g_str_equal (*attr_names, "name"))
        {
	  /* skip if empty */
	  if (**attr_values)
	    name = *attr_values;
	} else if (g_str_equal (*attr_names, "value")) {
	  if (**attr_values) {
	    value = (int) g_ascii_strtoull (*attr_values, NULL, 0);
	  }
	} else if (g_str_equal (*attr_names, "key")) {
	  if (**attr_values) {
	    gconf_key = *attr_values;
	  }
	} else if (g_str_equal (*attr_names, "comparison")) {
	  if (**attr_values) {
	    if (g_str_equal (*attr_values, "gt")) {
	      comparison = COMPARISON_GT;
	    } else if (g_str_equal (*attr_values, "lt")) {
	      comparison = COMPARISON_LT;
	    } else if (g_str_equal (*attr_values, "eq")) {
	      comparison = COMPARISON_EQ;
	    }
	  }
	}

      ++attr_names;
      ++attr_values;
    }

  if (name == NULL)
    return;

  key.name = g_strdup (name);
  key.description_name = NULL;
  key.value = value;
  if (gconf_key)
    key.key = g_strdup (gconf_key);
  else
    key.key = NULL;
  key.comparison = comparison;
  key.cmd_name = NULL;
  g_array_append_val (keylist->entries, key);
}

static gboolean
strv_contains (char **strv,
	       char  *str)
{
  char **p = strv;
  for (p = strv; *p; p++)
    if (strcmp (*p, str) == 0)
      return TRUE;

  return FALSE;
}

static void
append_sections_from_file (GtkBuilder *builder, const gchar *path, gchar **wm_keybindings)
{
  GError *err = NULL;
  char *buf;
  gsize buf_len;
  KeyList *keylist;
  KeyListEntry key, *keys;
  const char *title;
  guint i;
  GMarkupParseContext *ctx;
  GMarkupParser parser = { parse_start_tag, NULL, NULL, NULL, NULL };

  /* Parse file */
  if (!g_file_get_contents (path, &buf, &buf_len, &err))
    return;

  keylist = g_new0 (KeyList, 1);
  keylist->entries = g_array_new (FALSE, TRUE, sizeof (KeyListEntry));
  ctx = g_markup_parse_context_new (&parser, 0, keylist, NULL);

  if (!g_markup_parse_context_parse (ctx, buf, buf_len, &err))
    {
      g_warning ("Failed to parse '%s': '%s'", path, err->message);
      g_error_free (err);
      g_free (keylist->name);
      g_free (keylist->package);
      g_free (keylist->wm_name);
      for (i = 0; i < keylist->entries->len; i++)
	g_free (((KeyListEntry *) &(keylist->entries->data[i]))->name);
      g_array_free (keylist->entries, TRUE);
      g_free (keylist);
      keylist = NULL;
    }
  g_markup_parse_context_free (ctx);
  g_free (buf);

  if (keylist == NULL)
    return;

  /* If there's no keys to add, or the settings apply to a window manager
   * that's not the one we're running */
  if (keylist->entries->len == 0
      || (keylist->wm_name != NULL && !strv_contains (wm_keybindings, keylist->wm_name))
      || keylist->name == NULL)
    {
      g_free (keylist->name);
      g_free (keylist->package);
      g_free (keylist->wm_name);
      g_array_free (keylist->entries, TRUE);
      g_free (keylist);
      return;
    }

  /* Empty KeyListEntry to end the array */
  key.name = NULL;
  key.description_name = NULL;
  key.key = NULL;
  key.value = 0;
  key.comparison = COMPARISON_NONE;
  g_array_append_val (keylist->entries, key);

  keys = (KeyListEntry *) g_array_free (keylist->entries, FALSE);
  if (keylist->package)
    {
      bind_textdomain_codeset (keylist->package, "UTF-8");
      title = dgettext (keylist->package, keylist->name);
    } else {
      title = _(keylist->name);
    }

  append_section (builder, title, keys);

  g_free (keylist->name);
  g_free (keylist->package);

  for (i = 0; keys[i].name != NULL; i++)
    g_free (keys[i].name);

  g_free (keylist);
}

static void
append_sections_from_gconf (GtkBuilder *builder, const gchar *gconf_path)
{
  GConfClient *client;
  GSList *custom_list, *l;
  GArray *entries;
  KeyListEntry key;

  /* load custom shortcuts from GConf */
  entries = g_array_new (FALSE, TRUE, sizeof (KeyListEntry));

  key.key = NULL;
  key.value = 0;
  key.comparison = COMPARISON_NONE;

  client = gconf_client_get_default ();
  custom_list = gconf_client_all_dirs (client, gconf_path, NULL);

  for (l = custom_list; l != NULL; l = l->next)
    {
      key.name = g_strconcat (l->data, "/binding", NULL);
      key.cmd_name = g_strconcat (l->data, "/action", NULL);
      key.description_name = g_strconcat (l->data, "/name", NULL);
      g_array_append_val (entries, key);

      g_free (l->data);
    }

  g_slist_free (custom_list);
  g_object_unref (client);

  if (entries->len > 0)
    {
      KeyListEntry *keys;
      int i;

      /* Empty KeyListEntry to end the array */
      key.name = NULL;
      key.description_name = NULL;
      g_array_append_val (entries, key);

      keys = (KeyListEntry *) entries->data;
      append_section (builder, _("Custom Shortcuts"), keys);
      for (i = 0; i < entries->len; ++i)
        {
          g_free (keys[i].name);
          g_free (keys[i].description_name);
        }
    }

  g_array_free (entries, TRUE);
}

static void
reload_sections (GtkBuilder *builder)
{
  gchar **wm_keybindings;
  GDir *dir;
  const gchar *name;
  GtkTreeModel *section_model, *shortcut_model;

  section_model = gtk_tree_view_get_model (GTK_TREE_VIEW (gtk_builder_get_object (builder, "section_treeview")));
  shortcut_model = gtk_tree_view_get_model (GTK_TREE_VIEW (gtk_builder_get_object (builder, "shortcut_treeview")));
  /* FIXME: get current selection and keep it after refreshing */

  /* Clear previous models */
  gtk_list_store_clear (GTK_LIST_STORE (section_model));
  gtk_list_store_clear (GTK_LIST_STORE (shortcut_model));

  /* Load WM keybindings */
  wm_keybindings = wm_common_get_current_keybindings ();

  dir = g_dir_open (GNOMECC_KEYBINDINGS_DIR, 0, NULL);
  if (!dir)
      return;

  for (name = g_dir_read_name (dir) ; name ; name = g_dir_read_name (dir))
    {
      if (g_str_has_suffix (name, ".xml"))
        {
	  gchar *path;

	  path = g_build_filename (GNOMECC_KEYBINDINGS_DIR, name, NULL);
	  append_sections_from_file (builder, path, wm_keybindings);

	  g_free (path);
	}
    }

  g_dir_close (dir);
  g_strfreev (wm_keybindings);

  /* Load custom keybindings */
  append_sections_from_gconf (builder, GCONF_BINDING_DIR);
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
    g_object_set (cell,
		  "visible", FALSE,
		  NULL);
  else if (! key_entry->editable)
    g_object_set (cell,
		  "visible", TRUE,
		  "editable", FALSE,
		  "accel_key", key_entry->keyval,
		  "accel_mask", key_entry->mask,
		  "keycode", key_entry->keycode,
		  "style", PANGO_STYLE_ITALIC,
		  NULL);
  else
    g_object_set (cell,
		  "visible", TRUE,
		  "editable", TRUE,
		  "accel_key", key_entry->keyval,
		  "accel_mask", key_entry->mask,
		  "keycode", key_entry->keycode,
		  "style", PANGO_STYLE_NORMAL,
		  NULL);
}

static void
description_set_func (GtkTreeViewColumn *tree_column,
                      GtkCellRenderer   *cell,
                      GtkTreeModel      *model,
                      GtkTreeIter       *iter,
                      gpointer           data)
{
  KeyEntry *key_entry;

  gtk_tree_model_get (model, iter,
                      KEYENTRY_COLUMN, &key_entry,
                      -1);

  if (key_entry != NULL)
    g_object_set (cell,
		  "editable", FALSE,
		  "text", key_entry->description != NULL ?
			  key_entry->description : _("<Unknown Action>"),
		  NULL);
  else
    g_object_set (cell,
		  "editable", FALSE, NULL);
}

static void
section_selection_changed (GtkTreeSelection *selection, gpointer data)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkBuilder *builder = GTK_BUILDER (data);

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      GPtrArray *keys;
      GtkWidget *shortcut_treeview;
      GtkTreeModel *shortcut_model;
      gchar *description;
      gint i;

      gtk_tree_model_get (model, &iter, DESCRIPTION_COLUMN, &description, -1);
      keys = g_hash_table_lookup (kb_sections, description);
      if (keys == NULL)
        {
	  g_warning ("Can't find section %s in sections hash table!!!", description);
	  return;
	}

      /* Fill the shortcut treeview with the keys for the selected section */
      shortcut_treeview = GTK_WIDGET (gtk_builder_get_object (builder, "shortcut_treeview"));
      shortcut_model = gtk_tree_view_get_model (GTK_TREE_VIEW (shortcut_treeview));
      gtk_list_store_clear (GTK_LIST_STORE (shortcut_model));

      for (i = 0; i < keys->len; i++)
        {
	  GtkTreeIter new_row;
	  KeyEntry *entry = g_ptr_array_index (keys, i);

	  g_print ("Adding gconf: %s, keyval = %d\n",
		   entry->gconf_key, entry->keyval);

	  gtk_list_store_append (GTK_LIST_STORE (shortcut_model), &new_row);
	  gtk_list_store_set (GTK_LIST_STORE (shortcut_model), &new_row,
			      DESCRIPTION_COLUMN, entry->description,
			      KEYENTRY_COLUMN, entry,
			      -1);
	}
    }
}

static void
shortcut_selection_changed (GtkTreeSelection *selection, gpointer data)
{
  GtkWidget *button = data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  KeyEntry *key;
  gboolean can_remove;

  can_remove = FALSE;
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter, KEYENTRY_COLUMN, &key, -1);
      if (key && key->command != NULL && key->editable)
	can_remove = TRUE;
    }

  gtk_widget_set_sensitive (button, can_remove);
}

static void
setup_dialog (CcPanel *panel, GtkBuilder *builder)
{
  GConfClient *client;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkWidget *widget;
  GtkTreeView *treeview;
  GtkTreeSelection *selection;
  GSList *allowed_keys;
  CcShell *shell;
  GtkListStore *model;

  /* Setup the section treeview */
  treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder, "section_treeview"));

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Section"),
						     renderer,
						     "text", DESCRIPTION_COLUMN,
						     NULL);
  gtk_tree_view_append_column (treeview, column);

  model = gtk_list_store_new (1, G_TYPE_STRING);
  gtk_tree_view_set_model (treeview, GTK_TREE_MODEL (model));

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  g_signal_connect (selection, "changed",
		    G_CALLBACK (section_selection_changed), builder);

  /* Setup the shortcut treeview */
  treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder,
                                                    "shortcut_treeview"));

  client = gconf_client_get_default ();

  /* g_signal_connect (treeview, "button_press_event", */
  /* 		    G_CALLBACK (start_editing_cb), builder); */
  /* g_signal_connect (treeview, "row-activated", */
  /* 		    G_CALLBACK (start_editing_kb_cb), NULL); */

  renderer = gtk_cell_renderer_text_new ();

  /* g_signal_connect (renderer, "edited", */
  /*                   G_CALLBACK (description_edited_callback), */
  /*                   treeview); */

  column = gtk_tree_view_column_new_with_attributes (_("Action"),
						     renderer,
						     "text", DESCRIPTION_COLUMN,
						     NULL);
  gtk_tree_view_column_set_cell_data_func (column, renderer, description_set_func, NULL, NULL);
  gtk_tree_view_column_set_resizable (column, FALSE);

  gtk_tree_view_append_column (treeview, column);
  gtk_tree_view_column_set_sort_column_id (column, DESCRIPTION_COLUMN);

  renderer = (GtkCellRenderer *) g_object_new (EGG_TYPE_CELL_RENDERER_KEYS,
					       "accel_mode", EGG_CELL_RENDERER_KEYS_MODE_X,
					       NULL);

  /* g_signal_connect (renderer, "accel_edited", */
  /*                   G_CALLBACK (accel_edited_callback), */
  /*                   treeview); */

  /* g_signal_connect (renderer, "accel_cleared", */
  /*                   G_CALLBACK (accel_cleared_callback), */
  /*                   treeview); */

  column = gtk_tree_view_column_new_with_attributes (_("Shortcut"), renderer, NULL);
  gtk_tree_view_column_set_cell_data_func (column, renderer, accel_set_func, NULL, NULL);
  gtk_tree_view_column_set_resizable (column, FALSE);

  gtk_tree_view_append_column (treeview, column);
  gtk_tree_view_column_set_sort_column_id (column, KEYENTRY_COLUMN);

  gconf_client_add_dir (client, GCONF_BINDING_DIR, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  gconf_client_add_dir (client, "/apps/metacity/general", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  /* gconf_client_notify_add (client, */
  /* 			   "/apps/metacity/general/num_workspaces", */
  /* 			   (GConfClientNotifyFunc) key_entry_controlling_key_changed, */
  /* 			   builder, NULL, NULL); */

  model = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER);
  gtk_tree_view_set_model (treeview, GTK_TREE_MODEL (model));

  /* set up the dialog */
  shell = cc_panel_get_shell (CC_PANEL (panel));
  widget = cc_shell_get_toplevel (shell);

  /* maybe_block_accels_id = g_signal_connect (widget, "key_press_event", */
  /* 					    G_CALLBACK (maybe_block_accels), NULL); */

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  g_signal_connect (selection, "changed",
		    G_CALLBACK (shortcut_selection_changed),
		    WID (builder, "remove-button"));

  allowed_keys = gconf_client_get_list (client,
                                        GCONF_BINDING_DIR "/allowed_keys",
                                        GCONF_VALUE_STRING,
                                        NULL);
  if (allowed_keys != NULL)
    {
      g_slist_foreach (allowed_keys, (GFunc)g_free, NULL);
      g_slist_free (allowed_keys);
      gtk_widget_set_sensitive (WID (builder, "add-button"),
                                FALSE);
    }

  g_object_unref (client);

  /* setup the custom shortcut dialog */
  custom_shortcut_dialog = WID (builder,
				"custom-shortcut-dialog");
  custom_shortcut_name_entry = WID (builder,
				    "custom-shortcut-name-entry");
  custom_shortcut_command_entry = WID (builder,
				       "custom-shortcut-command-entry");
  /* g_signal_connect (WID (builder, "add-button"), */
  /*                   "clicked", G_CALLBACK (add_button_clicked), builder); */
  /* g_signal_connect (WID (builder, "remove-button"), */
  /*                   "clicked", G_CALLBACK (remove_button_clicked), builder); */

  gtk_dialog_set_default_response (GTK_DIALOG (custom_shortcut_dialog),
				   GTK_RESPONSE_OK);

  gtk_window_set_transient_for (GTK_WINDOW (custom_shortcut_dialog),
                                GTK_WINDOW (widget));
}

void
keyboard_shortcuts_init (CcPanel *panel, GtkBuilder *builder)
{
  kb_sections = g_hash_table_new_full (g_str_hash, g_str_equal,
				       g_free, (GDestroyNotify) free_key_array);
  setup_dialog (panel, builder);
  reload_sections (builder);
}

void
keyboard_shortcuts_dispose (CcPanel *panel)
{
}
