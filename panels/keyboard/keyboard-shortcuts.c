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

#include <config.h>

#include <glib/gi18n.h>

#include "keyboard-shortcuts.h"
#include "cc-keyboard-item.h"
#include "cc-keyboard-option.h"
#include "wm-common.h"

#define BINDINGS_SCHEMA "org.gnome.settings-daemon.plugins.media-keys"
#define CUSTOM_KEYS_BASENAME "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings"
#define CUSTOM_SHORTCUTS_ID "custom"
#define WID(builder, name) (GTK_WIDGET (gtk_builder_get_object (builder, name)))

typedef struct {
  /* The untranslated name, combine with ->package to translate */
  char *name;
  /* The group of keybindings (system or application) */
  char *group;
  /* The gettext package to use to translate the section title */
  char *package;
  /* Name of the window manager the keys would apply to */
  char *wm_name;
  /* The GSettings schema for the whole file, if any */
  char *schema;
  /* an array of KeyListEntry */
  GArray *entries;
} KeyList;

typedef struct
{
  CcKeyboardItemType type;
  char *schema; /* GSettings schema name, if any */
  char *description; /* description for GSettings types */
  char *gettext_package;
  char *name; /* GSettings schema path, or GSettings key name depending on type */
} KeyListEntry;

typedef enum
{
  SHORTCUT_TYPE_KEY_ENTRY,
  SHORTCUT_TYPE_XKB_OPTION,
} ShortcutType;

enum
{
  DETAIL_DESCRIPTION_COLUMN,
  DETAIL_KEYENTRY_COLUMN,
  DETAIL_TYPE_COLUMN,
  DETAIL_N_COLUMNS
};

enum
{
  SECTION_DESCRIPTION_COLUMN,
  SECTION_ID_COLUMN,
  SECTION_GROUP_COLUMN,
  SECTION_N_COLUMNS
};

static GSettings *binding_settings = NULL;
static GtkWidget *custom_shortcut_dialog = NULL;
static GtkWidget *custom_shortcut_name_entry = NULL;
static GtkWidget *custom_shortcut_command_entry = NULL;
static GHashTable *kb_system_sections = NULL;
static GHashTable *kb_apps_sections = NULL;
static GHashTable *kb_user_sections = NULL;

static void
free_key_array (GPtrArray *keys)
{
  if (keys != NULL)
    {
      gint i;

      for (i = 0; i < keys->len; i++)
        {
          CcKeyboardItem *item;

          item = g_ptr_array_index (keys, i);

          g_object_unref (item);
        }

      g_ptr_array_free (keys, TRUE);
    }
}

static GHashTable *
get_hash_for_group (BindingGroupType group)
{
  GHashTable *hash;

  switch (group)
    {
    case BINDING_GROUP_SYSTEM:
      hash = kb_system_sections;
      break;
    case BINDING_GROUP_APPS:
      hash = kb_apps_sections;
      break;
    case BINDING_GROUP_USER:
      hash = kb_user_sections;
      break;
    default:
      hash = NULL;
    }
  return hash;
}

static gboolean
have_key_for_group (int group, const gchar *name)
{
  GHashTableIter iter;
  GPtrArray *keys;
  gint i;

  g_hash_table_iter_init (&iter, get_hash_for_group (group));
  while (g_hash_table_iter_next (&iter, NULL, (gpointer *)&keys))
    {
      for (i = 0; i < keys->len; i++)
        {
          CcKeyboardItem *item = g_ptr_array_index (keys, i);

	  if (item->type == CC_KEYBOARD_ITEM_TYPE_GSETTINGS &&
	      g_strcmp0 (name, item->key) == 0)
	    {
	          return TRUE;
	    }

	  return FALSE;
        }
    }

  return FALSE;
}

static gboolean
keybinding_key_changed_foreach (GtkTreeModel   *model,
                                GtkTreePath    *path,
                                GtkTreeIter    *iter,
                                CcKeyboardItem *item)
{
  CcKeyboardItem *tmp_item;

  gtk_tree_model_get (item->model, iter,
                      DETAIL_KEYENTRY_COLUMN, &tmp_item,
                      -1);

  if (item == tmp_item)
    {
      gtk_tree_model_row_changed (item->model, path, iter);
      return TRUE;
    }
  return FALSE;
}

static void
item_changed (CcKeyboardItem *item,
	      GParamSpec     *pspec,
	      gpointer        user_data)
{
  /* update the model */
  gtk_tree_model_foreach (item->model, (GtkTreeModelForeachFunc) keybinding_key_changed_foreach, item);
}


static void
append_section (GtkBuilder         *builder,
                const gchar        *title,
                const gchar        *id,
                BindingGroupType    group,
                const KeyListEntry *keys_list)
{
  GPtrArray *keys_array;
  GtkTreeModel *sort_model;
  GtkTreeModel *model, *shortcut_model;
  GtkTreeIter iter;
  gint i;
  GHashTable *hash;
  gboolean is_new;

  hash = get_hash_for_group (group);
  if (!hash)
    return;

  sort_model = gtk_tree_view_get_model (GTK_TREE_VIEW (gtk_builder_get_object (builder, "section_treeview")));
  model = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (sort_model));

  shortcut_model = gtk_tree_view_get_model (GTK_TREE_VIEW (gtk_builder_get_object (builder, "shortcut_treeview")));

  /* Add all CcKeyboardItems for this section */
  is_new = FALSE;
  keys_array = g_hash_table_lookup (hash, id);
  if (keys_array == NULL)
    {
      keys_array = g_ptr_array_new ();
      is_new = TRUE;
    }

  for (i = 0; keys_list != NULL && keys_list[i].name != NULL; i++)
    {
      CcKeyboardItem *item;
      gboolean ret;

      if (have_key_for_group (group, keys_list[i].name))
        continue;

      item = cc_keyboard_item_new (keys_list[i].type);
      switch (keys_list[i].type)
        {
	case CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH:
          ret = cc_keyboard_item_load_from_gsettings_path (item, keys_list[i].name, FALSE);
          break;
	case CC_KEYBOARD_ITEM_TYPE_GSETTINGS:
	  ret = cc_keyboard_item_load_from_gsettings (item,
						      keys_list[i].description,
						      keys_list[i].schema,
						      keys_list[i].name);
	  break;
	default:
	  g_assert_not_reached ();
	}

      if (ret == FALSE)
        {
          /* We don't actually want to popup a dialog - just skip this one */
          g_object_unref (item);
          continue;
        }

      item->model = shortcut_model;
      item->group = group;

      g_signal_connect (G_OBJECT (item), "notify",
			G_CALLBACK (item_changed), NULL);

      g_ptr_array_add (keys_array, item);
    }

  /* Add the keys to the hash table */
  if (is_new)
    {
      g_hash_table_insert (hash, g_strdup (id), keys_array);

      /* Append the section to the left tree view */
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          SECTION_DESCRIPTION_COLUMN, title,
                          SECTION_ID_COLUMN, id,
                          SECTION_GROUP_COLUMN, group,
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
  const char *name, *schema, *description, *package;

  name = NULL;
  schema = NULL;
  package = NULL;

  /* The top-level element, names the section in the tree */
  if (g_str_equal (element_name, "KeyListEntries"))
    {
      const char *wm_name = NULL;
      const char *group = NULL;

      while (*attr_names && *attr_values)
        {
          if (g_str_equal (*attr_names, "name"))
            {
              if (**attr_values)
                name = *attr_values;
            } else if (g_str_equal (*attr_names, "group")) {
              if (**attr_values)
                group = *attr_values;
            } else if (g_str_equal (*attr_names, "wm_name")) {
              if (**attr_values)
                wm_name = *attr_values;
	    } else if (g_str_equal (*attr_names, "schema")) {
	      if (**attr_values)
	        schema = *attr_values;
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
	  bind_textdomain_codeset (keylist->package, "UTF-8");
        }
      if (group)
        {
          if (keylist->group)
            g_warning ("Duplicate group");
          g_free (keylist->group);
          keylist->group = g_strdup (group);
        }
      if (schema)
        {
          if (keylist->schema)
            g_warning ("Duplicate schema");
          g_free (keylist->schema);
          keylist->schema = g_strdup (schema);
	}
      return;
    }

  if (!g_str_equal (element_name, "KeyListEntry")
      || attr_names == NULL
      || attr_values == NULL)
    return;

  schema = NULL;
  description = NULL;

  while (*attr_names && *attr_values)
    {
      if (g_str_equal (*attr_names, "name"))
        {
          /* skip if empty */
          if (**attr_values)
            name = *attr_values;
	} else if (g_str_equal (*attr_names, "schema")) {
	  if (**attr_values) {
	   schema = *attr_values;
	  }
	} else if (g_str_equal (*attr_names, "description")) {
          if (**attr_values) {
            if (keylist->package)
	      {
	        description = dgettext (keylist->package, *attr_values);
	      }
	    else
	      {
	        description = _(*attr_values);
	      }
	  }
        }

      ++attr_names;
      ++attr_values;
    }

  if (name == NULL)
    return;

  if (schema == NULL &&
      keylist->schema == NULL) {
    g_debug ("Ignored GConf keyboard shortcut '%s'", name);
    return;
  }

  key.name = g_strdup (name);
  key.type = CC_KEYBOARD_ITEM_TYPE_GSETTINGS;
  key.description = g_strdup (description);
  key.gettext_package = g_strdup (keylist->package);
  key.schema = schema ? g_strdup (schema) : g_strdup (keylist->schema);
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
append_sections_from_file (GtkBuilder *builder, const gchar *path, const char *datadir, gchar **wm_keybindings)
{
  GError *err = NULL;
  char *buf;
  gsize buf_len;
  KeyList *keylist;
  KeyListEntry key, *keys;
  const char *title;
  int group;
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
  g_array_append_val (keylist->entries, key);

  keys = (KeyListEntry *) g_array_free (keylist->entries, FALSE);
  if (keylist->package)
    {
      char *localedir;

      localedir = g_build_filename (datadir, "locale", NULL);
      bindtextdomain (keylist->package, localedir);
      g_free (localedir);

      title = dgettext (keylist->package, keylist->name);
    } else {
      title = _(keylist->name);
    }
  if (keylist->group && strcmp (keylist->group, "system") == 0)
    group = BINDING_GROUP_SYSTEM;
  else
    group = BINDING_GROUP_APPS;

  append_section (builder, title, keylist->name, group, keys);

  g_free (keylist->name);
  g_free (keylist->package);
  g_free (keylist->wm_name);
  g_free (keylist->schema);
  g_free (keylist->group);

  for (i = 0; keys[i].name != NULL; i++) {
    KeyListEntry *entry = &keys[i];
    g_free (entry->schema);
    g_free (entry->description);
    g_free (entry->gettext_package);
    g_free (entry->name);
  }

  g_free (keylist);
}

static void
append_sections_from_gsettings (GtkBuilder *builder)
{
  char **custom_paths;
  GArray *entries;
  KeyListEntry key;
  int i;

  /* load custom shortcuts from GSettings */
  entries = g_array_new (FALSE, TRUE, sizeof (KeyListEntry));

  custom_paths = g_settings_get_strv (binding_settings, "custom-keybindings");
  for (i = 0; custom_paths[i]; i++)
    {
      key.name = g_strdup (custom_paths[i]);
      if (!have_key_for_group (BINDING_GROUP_USER, key.name))
        {
          key.type = CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH;
          g_array_append_val (entries, key);
        }
      else
        g_free (key.name);
    }
  g_strfreev (custom_paths);

  if (entries->len > 0)
    {
      KeyListEntry *keys;
      int i;

      /* Empty KeyListEntry to end the array */
      key.name = NULL;
      g_array_append_val (entries, key);

      keys = (KeyListEntry *) entries->data;
      append_section (builder, _("Custom Shortcuts"), CUSTOM_SHORTCUTS_ID, BINDING_GROUP_USER, keys);
      for (i = 0; i < entries->len; ++i)
        {
          g_free (keys[i].name);
        }
    }
  else
    {
      append_section (builder, _("Custom Shortcuts"), CUSTOM_SHORTCUTS_ID, BINDING_GROUP_USER, NULL);
    }

  g_array_free (entries, TRUE);
}

static void
reload_sections (CcPanel *panel)
{
  GtkBuilder *builder;
  gchar **wm_keybindings;
  GDir *dir;
  GtkTreeModel *sort_model;
  GtkTreeModel *section_model;
  GtkTreeModel *shortcut_model;
  const gchar * const * data_dirs;
  guint i;
  GtkTreeView *section_treeview;
  GtkTreeSelection *selection;
  GtkTreeIter iter;
  GHashTable *loaded_files;
  const char *section_to_set;

  builder = g_object_get_data (G_OBJECT (panel), "builder");

  section_treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder, "section_treeview"));
  sort_model = gtk_tree_view_get_model (section_treeview);
  section_model = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (sort_model));

  shortcut_model = gtk_tree_view_get_model (GTK_TREE_VIEW (gtk_builder_get_object (builder, "shortcut_treeview")));
  /* FIXME: get current selection and keep it after refreshing */

  /* Clear previous models and hash tables */
  gtk_list_store_clear (GTK_LIST_STORE (section_model));
  gtk_list_store_clear (GTK_LIST_STORE (shortcut_model));
  if (kb_system_sections != NULL)
    g_hash_table_destroy (kb_system_sections);
  kb_system_sections = g_hash_table_new_full (g_str_hash,
                                              g_str_equal,
                                              g_free,
                                              (GDestroyNotify) free_key_array);

  if (kb_apps_sections != NULL)
    g_hash_table_destroy (kb_apps_sections);
  kb_apps_sections = g_hash_table_new_full (g_str_hash,
                                            g_str_equal,
                                            g_free,
                                            (GDestroyNotify) free_key_array);

  if (kb_user_sections != NULL)
    g_hash_table_destroy (kb_user_sections);
  kb_user_sections = g_hash_table_new_full (g_str_hash,
                                            g_str_equal,
                                            g_free,
                                            (GDestroyNotify) free_key_array);

  /* Load WM keybindings */
  wm_keybindings = wm_common_get_current_keybindings ();

  loaded_files = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  data_dirs = g_get_system_data_dirs ();
  for (i = 0; data_dirs[i] != NULL; i++)
    {
      char *dir_path;
      const gchar *name;

      dir_path = g_build_filename (data_dirs[i], "gnome-control-center", "keybindings", NULL);

      dir = g_dir_open (dir_path, 0, NULL);
      if (!dir)
        {
          g_free (dir_path);
          continue;
        }

      for (name = g_dir_read_name (dir) ; name ; name = g_dir_read_name (dir))
        {
          gchar *path;

	  if (g_str_has_suffix (name, ".xml") == FALSE)
	    continue;

          if (g_hash_table_lookup (loaded_files, name) != NULL)
	    {
	      g_debug ("Not loading %s, it was already loaded from another directory", name);
              continue;
	    }

	  g_hash_table_insert (loaded_files, g_strdup (name), GINT_TO_POINTER (1));
	  path = g_build_filename (dir_path, name, NULL);
	  append_sections_from_file (builder, path, data_dirs[i], wm_keybindings);
	  g_free (path);
	}
      g_free (dir_path);
      g_dir_close (dir);
    }

  g_hash_table_destroy (loaded_files);
  g_strfreev (wm_keybindings);

  /* Add a separator */
  gtk_list_store_append (GTK_LIST_STORE (section_model), &iter);
  gtk_list_store_set (GTK_LIST_STORE (section_model), &iter,
                      SECTION_DESCRIPTION_COLUMN, NULL,
                      SECTION_GROUP_COLUMN, BINDING_GROUP_SEPARATOR,
                      -1);

  /* Load custom keybindings */
  append_sections_from_gsettings (builder);

  /* Select the first item, or the requested section, if any */
  section_to_set = g_object_get_data (G_OBJECT (panel), "section-to-set");
  if (section_to_set != NULL)
    {
      if (keyboard_shortcuts_set_section (panel, section_to_set))
        {
          g_object_set_data (G_OBJECT (panel), "section-to-set", NULL);
          return;
	}
    }
  gtk_tree_model_get_iter_first (sort_model, &iter);
  selection = gtk_tree_view_get_selection (section_treeview);
  gtk_tree_selection_select_iter (selection, &iter);

  g_object_set_data (G_OBJECT (panel), "section-to-set", NULL);
}

static void
accel_set_func (GtkTreeViewColumn *tree_column,
                GtkCellRenderer   *cell,
                GtkTreeModel      *model,
                GtkTreeIter       *iter,
                gpointer           data)
{
  gpointer entry;
  ShortcutType type;

  gtk_tree_model_get (model, iter,
                      DETAIL_KEYENTRY_COLUMN, &entry,
                      DETAIL_TYPE_COLUMN, &type,
                      -1);

  gtk_cell_renderer_set_visible (cell, FALSE);

  if (type == SHORTCUT_TYPE_XKB_OPTION &&
      GTK_IS_CELL_RENDERER_COMBO (cell))
    {
      CcKeyboardOption *option = entry;

      gtk_cell_renderer_set_visible (cell, TRUE);
      g_object_set (cell,
                    "model", cc_keyboard_option_get_store (option),
                    "text", cc_keyboard_option_get_current_value_description (option),
                    NULL);
    }
  else if (type == SHORTCUT_TYPE_KEY_ENTRY &&
           GTK_IS_CELL_RENDERER_TEXT (cell) &&
           !GTK_IS_CELL_RENDERER_COMBO (cell) &&
           entry != NULL)
    {
      CcKeyboardItem *item = entry;

      gtk_cell_renderer_set_visible (cell, TRUE);

      if (item->editable)
        g_object_set (cell,
                      "editable", TRUE,
                      "accel-key", item->keyval,
                      "accel-mods", item->mask,
                      "keycode", item->keycode,
                      "style", PANGO_STYLE_NORMAL,
                      NULL);
      else
        g_object_set (cell,
                      "editable", FALSE,
                      "accel-key", item->keyval,
                      "accel-mods", item->mask,
                      "keycode", item->keycode,
                      "style", PANGO_STYLE_ITALIC,
                      NULL);
    }
}

static void
description_set_func (GtkTreeViewColumn *tree_column,
                      GtkCellRenderer   *cell,
                      GtkTreeModel      *model,
                      GtkTreeIter       *iter,
                      gpointer           data)
{
  gchar *description;
  CcKeyboardItem *item;
  ShortcutType type;

  gtk_tree_model_get (model, iter,
                      DETAIL_DESCRIPTION_COLUMN, &description,
                      DETAIL_KEYENTRY_COLUMN, &item,
                      DETAIL_TYPE_COLUMN, &type,
                      -1);

  if (type == SHORTCUT_TYPE_XKB_OPTION)
    {
      g_object_set (cell, "text", description, NULL);
    }
  else
    {
      if (item != NULL)
        g_object_set (cell,
                      "editable", FALSE,
                      "text", item->description != NULL ?
                      item->description : _("<Unknown Action>"),
                      NULL);
      else
        g_object_set (cell,
                      "editable", FALSE, NULL);
    }

  g_free (description);
}

static void
shortcut_selection_changed (GtkTreeSelection *selection, gpointer data)
{
  GtkWidget *button = data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  CcKeyboardItem *item;
  gboolean can_remove;
  ShortcutType type;

  can_remove = FALSE;
  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      gtk_tree_model_get (model, &iter,
                          DETAIL_KEYENTRY_COLUMN, &item,
                          DETAIL_TYPE_COLUMN, &type,
                          -1);
      if (type == SHORTCUT_TYPE_KEY_ENTRY &&
          item && item->command != NULL && item->editable)
        can_remove = TRUE;
    }

  gtk_widget_set_sensitive (button, can_remove);
}

static void
fill_xkb_options_shortcuts (GtkTreeModel *model)
{
  GList *l;
  GtkTreeIter iter;

  for (l = cc_keyboard_option_get_all (); l; l = l->next)
    {
      CcKeyboardOption *option = l->data;

      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          DETAIL_DESCRIPTION_COLUMN, cc_keyboard_option_get_description (option),
                          DETAIL_KEYENTRY_COLUMN, option,
                          DETAIL_TYPE_COLUMN, SHORTCUT_TYPE_XKB_OPTION,
                          -1);
    }
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
      gchar *id;
      BindingGroupType group;
      gint i;

      gtk_tree_model_get (model, &iter,
                          SECTION_ID_COLUMN, &id,
                          SECTION_GROUP_COLUMN, &group, -1);

      keys = g_hash_table_lookup (get_hash_for_group (group), id);
      if (keys == NULL)
        {
          g_warning ("Can't find section %s in sections hash table.", id);
          g_free (id);
          return;
        }

      gtk_widget_set_sensitive (WID (builder, "remove-toolbutton"), FALSE);

      /* Fill the shortcut treeview with the keys for the selected section */
      shortcut_treeview = GTK_WIDGET (gtk_builder_get_object (builder, "shortcut_treeview"));
      shortcut_model = gtk_tree_view_get_model (GTK_TREE_VIEW (shortcut_treeview));
      gtk_list_store_clear (GTK_LIST_STORE (shortcut_model));

      for (i = 0; i < keys->len; i++)
        {
          GtkTreeIter new_row;
          CcKeyboardItem *item = g_ptr_array_index (keys, i);

          gtk_list_store_append (GTK_LIST_STORE (shortcut_model), &new_row);
          gtk_list_store_set (GTK_LIST_STORE (shortcut_model), &new_row,
                              DETAIL_DESCRIPTION_COLUMN, item->description,
                              DETAIL_KEYENTRY_COLUMN, item,
                              DETAIL_TYPE_COLUMN, SHORTCUT_TYPE_KEY_ENTRY,
                              -1);
        }

      if (g_str_equal (id, "Typing"))
        fill_xkb_options_shortcuts (shortcut_model);

      g_free (id);
    }
}

static gboolean
edit_custom_shortcut (CcKeyboardItem *item)
{
  gint result;
  gboolean ret;
  GSettings *settings;

  settings = g_settings_new_with_path (item->schema, item->gsettings_path);

  g_settings_bind (settings, "name",
                   G_OBJECT (custom_shortcut_name_entry), "text",
                   G_SETTINGS_BIND_DEFAULT);
  gtk_widget_grab_focus (custom_shortcut_name_entry);

  g_settings_bind (settings, "command",
                   G_OBJECT (custom_shortcut_command_entry), "text",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_delay (settings);

  gtk_window_present (GTK_WINDOW (custom_shortcut_dialog));
  result = gtk_dialog_run (GTK_DIALOG (custom_shortcut_dialog));
  switch (result)
    {
    case GTK_RESPONSE_OK:
      g_settings_apply (settings);
      ret = TRUE;
      break;
    default:
      g_settings_revert (settings);
      ret = FALSE;
      break;
    }

  g_settings_unbind (G_OBJECT (custom_shortcut_name_entry), "text");
  g_settings_unbind (G_OBJECT (custom_shortcut_command_entry), "text");

  g_object_unref (settings);

  gtk_widget_hide (custom_shortcut_dialog);

  return ret;
}

static gboolean
remove_custom_shortcut (GtkTreeModel *model, GtkTreeIter *iter)
{
  CcKeyboardItem *item;
  GPtrArray *keys_array;
  GVariantBuilder builder;
  char **settings_paths;
  int i;

  gtk_tree_model_get (model, iter,
                      DETAIL_KEYENTRY_COLUMN, &item,
                      -1);

  /* not a custom shortcut */
  g_assert (item->type == CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH);

  g_settings_delay (item->settings);
  g_settings_reset (item->settings, "name");
  g_settings_reset (item->settings, "command");
  g_settings_reset (item->settings, "binding");
  g_settings_apply (item->settings);
  g_settings_sync ();

  settings_paths = g_settings_get_strv (binding_settings, "custom-keybindings");
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
  for (i = 0; settings_paths[i]; i++)
    if (strcmp (settings_paths[i], item->gsettings_path) != 0)
      g_variant_builder_add (&builder, "s", settings_paths[i]);
  g_settings_set_value (binding_settings,
                        "custom-keybindings", g_variant_builder_end (&builder));
  g_strfreev (settings_paths);
  g_object_unref (item);

  keys_array = g_hash_table_lookup (get_hash_for_group (BINDING_GROUP_USER), CUSTOM_SHORTCUTS_ID);
  g_ptr_array_remove (keys_array, item);

  gtk_list_store_remove (GTK_LIST_STORE (model), iter);

  return TRUE;
}

static void
update_custom_shortcut (GtkTreeModel *model, GtkTreeIter *iter)
{
  CcKeyboardItem *item;

  gtk_tree_model_get (model, iter,
                      DETAIL_KEYENTRY_COLUMN, &item,
                      -1);

  g_assert (item->type == CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH);

  edit_custom_shortcut (item);
  if (item->command == NULL || item->command[0] == '\0')
    {
      remove_custom_shortcut (model, iter);
    }
  else
    {
      gtk_list_store_set (GTK_LIST_STORE (model), iter,
                          DETAIL_KEYENTRY_COLUMN, item, -1);
    }
}

static gboolean
start_editing_cb (GtkTreeView    *tree_view,
                  GdkEventButton *event,
                  gpointer        user_data)
{
  GtkTreePath *path;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell = user_data;

  if (event->window != gtk_tree_view_get_bin_window (tree_view))
    return FALSE;

  if (gtk_tree_view_get_path_at_pos (tree_view,
                                     (gint) event->x,
                                     (gint) event->y,
                                     &path, &column,
                                     NULL, NULL))
    {
      GtkTreeModel *model;
      GtkTreeIter iter;
      CcKeyboardItem *item;
      ShortcutType type;

      model = gtk_tree_view_get_model (tree_view);
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_model_get (model, &iter,
                          DETAIL_KEYENTRY_COLUMN, &item,
                          DETAIL_TYPE_COLUMN, &type,
                         -1);

      if (type == SHORTCUT_TYPE_XKB_OPTION)
        {
          gtk_tree_path_free (path);
          return FALSE;
        }

      /* if only the accel can be edited on the selected row
       * always select the accel column */
      if (item->desc_editable &&
          column == gtk_tree_view_get_column (tree_view, 0))
        {
          gtk_widget_grab_focus (GTK_WIDGET (tree_view));
          gtk_tree_view_set_cursor (tree_view,
                                    path,
                                    column,
                                    FALSE);
          update_custom_shortcut (model, &iter);
        }
      else
        {
          gtk_widget_grab_focus (GTK_WIDGET (tree_view));
          gtk_tree_view_set_cursor_on_cell (tree_view,
                                            path,
                                            gtk_tree_view_get_column (tree_view, 1),
                                            cell,
                                            TRUE);
        }
      g_signal_stop_emission_by_name (tree_view, "button_press_event");
      gtk_tree_path_free (path);
    }
  return TRUE;
}

static void
start_editing_kb_cb (GtkTreeView       *treeview,
                     GtkTreePath       *path,
                     GtkTreeViewColumn *column,
                     gpointer           user_data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  CcKeyboardItem *item;
  ShortcutType type;
  GtkCellRenderer *cell = user_data;

  model = gtk_tree_view_get_model (treeview);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter,
                      DETAIL_KEYENTRY_COLUMN, &item,
                      DETAIL_TYPE_COLUMN, &type,
                      -1);

  if (type == SHORTCUT_TYPE_XKB_OPTION)
    return;

  /* if only the accel can be edited on the selected row
   * always select the accel column */
  if (item->desc_editable &&
      column == gtk_tree_view_get_column (treeview, 0))
    {
      gtk_widget_grab_focus (GTK_WIDGET (treeview));
      gtk_tree_view_set_cursor (treeview,
                                path,
                                column,
                                FALSE);
      update_custom_shortcut (model, &iter);
    }
  else
    {
       gtk_widget_grab_focus (GTK_WIDGET (treeview));
       gtk_tree_view_set_cursor_on_cell (treeview,
                                         path,
                                         gtk_tree_view_get_column (treeview, 1),
                                         cell,
                                         TRUE);
    }
}

static const guint forbidden_keyvals[] = {
  /* Navigation keys */
  GDK_KEY_Home,
  GDK_KEY_Left,
  GDK_KEY_Up,
  GDK_KEY_Right,
  GDK_KEY_Down,
  GDK_KEY_Page_Up,
  GDK_KEY_Page_Down,
  GDK_KEY_End,
  GDK_KEY_Tab,

  /* Return */
  GDK_KEY_KP_Enter,
  GDK_KEY_Return,

  GDK_KEY_space,
  GDK_KEY_Mode_switch
};

static char*
binding_name (guint                   keyval,
              guint                   keycode,
              GdkModifierType         mask,
              gboolean                translate)
{
  if (keyval != 0 || keycode != 0)
    return translate ?
        gtk_accelerator_get_label_with_keycode (NULL, keyval, keycode, mask) :
        gtk_accelerator_name_with_keycode (NULL, keyval, keycode, mask);
  else
    return g_strdup (translate ? _("Disabled") : "");
}

static gboolean
keyval_is_forbidden (guint keyval)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS(forbidden_keyvals); i++) {
    if (keyval == forbidden_keyvals[i])
      return TRUE;
  }

  return FALSE;
}

typedef struct {
  CcKeyboardItem *orig_item;
  CcKeyboardItem *conflict_item;
  guint new_keyval;
  GdkModifierType new_mask;
  guint new_keycode;
} CcUniquenessData;

static gboolean
compare_keys_for_uniqueness (CcKeyboardItem   *element,
                             CcUniquenessData *data)
{
  CcKeyboardItem *orig_item;

  orig_item = data->orig_item;

  /* no conflict for : blanks, different modifiers, or ourselves */
  if (element == NULL || data->new_mask != element->mask ||
      cc_keyboard_item_equal (orig_item, element))
    return FALSE;

  if (data->new_keyval != 0) {
      if (data->new_keyval != element->keyval)
          return FALSE;
  } else if (element->keyval != 0 || data->new_keycode != element->keycode)
    return FALSE;

  data->conflict_item = element;

  return TRUE;
}

static gboolean
cb_check_for_uniqueness (gpointer          key,
                         GPtrArray        *keys_array,
                         CcUniquenessData *data)
{
  guint i;

  for (i = 0; i < keys_array->len; i++)
    {
      CcKeyboardItem *item;

      item = keys_array->pdata[i];
      if (compare_keys_for_uniqueness (item, data))
        return TRUE;
    }
  return FALSE;
}

static void
accel_edited_callback (GtkCellRendererText   *cell,
                       const char            *path_string,
                       guint                  keyval,
                       GdkModifierType        mask,
                       guint                  keycode,
                       GtkTreeView           *view)
{
  GtkTreeModel *model;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;
  CcUniquenessData data;
  CcKeyboardItem *item;
  char *str;

  model = gtk_tree_view_get_model (view);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);
  gtk_tree_model_get (model, &iter,
                      DETAIL_KEYENTRY_COLUMN, &item,
                      -1);

  /* sanity check */
  if (item == NULL)
    return;

  /* CapsLock isn't supported as a keybinding modifier, so keep it from confusing us */
  mask &= ~GDK_LOCK_MASK;

  data.orig_item = item;
  data.new_keyval = keyval;
  data.new_mask = mask;
  data.new_keycode = keycode;
  data.conflict_item = NULL;

  if (keyval != 0 || keycode != 0) /* any number of shortcuts can be disabled */
    {
      BindingGroupType i;

      for (i = BINDING_GROUP_SYSTEM; i <= BINDING_GROUP_USER && data.conflict_item == NULL; i++)
        {
          GHashTable *table;

          table = get_hash_for_group (i);
          if (!table)
            continue;
          g_hash_table_find (table, (GHRFunc) cb_check_for_uniqueness, &data);
        }
    }

  /* Check for unmodified keys */
  if ((mask == 0 || mask == GDK_SHIFT_MASK) && keycode != 0)
    {
      if ((keyval >= GDK_KEY_a && keyval <= GDK_KEY_z)
           || (keyval >= GDK_KEY_A && keyval <= GDK_KEY_Z)
           || (keyval >= GDK_KEY_0 && keyval <= GDK_KEY_9)
           || (keyval >= GDK_KEY_kana_fullstop && keyval <= GDK_KEY_semivoicedsound)
           || (keyval >= GDK_KEY_Arabic_comma && keyval <= GDK_KEY_Arabic_sukun)
           || (keyval >= GDK_KEY_Serbian_dje && keyval <= GDK_KEY_Cyrillic_HARDSIGN)
           || (keyval >= GDK_KEY_Greek_ALPHAaccent && keyval <= GDK_KEY_Greek_omega)
           || (keyval >= GDK_KEY_hebrew_doublelowline && keyval <= GDK_KEY_hebrew_taf)
           || (keyval >= GDK_KEY_Thai_kokai && keyval <= GDK_KEY_Thai_lekkao)
           || (keyval >= GDK_KEY_Hangul && keyval <= GDK_KEY_Hangul_Special)
           || (keyval >= GDK_KEY_Hangul_Kiyeog && keyval <= GDK_KEY_Hangul_J_YeorinHieuh)
           || keyval_is_forbidden (keyval)) {
        GtkWidget *dialog;
        char *name;

        name = binding_name (keyval, keycode, mask, TRUE);

        dialog =
          gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
                                  GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                  GTK_MESSAGE_WARNING,
                                  GTK_BUTTONS_CANCEL,
                                  _("The shortcut \"%s\" cannot be used because it will become impossible to type using this key.\n"
                                  "Please try with a key such as Control, Alt or Shift at the same time."),
                                  name);

        g_free (name);
        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);

        /* set it back to its previous value. */
        g_object_set (G_OBJECT (cell),
                      "accel-key", item->keyval,
                      "keycode", item->keycode,
                      "accel-mods", item->mask,
                      NULL);
        return;
      }
    }

  /* flag to see if the new accelerator was in use by something */
  if (data.conflict_item != NULL)
    {
      GtkWidget *dialog;
      char *name;
      int response;

      name = binding_name (keyval, keycode, mask, TRUE);

      dialog =
        gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
                                GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                GTK_MESSAGE_WARNING,
                                GTK_BUTTONS_CANCEL,
                                _("The shortcut \"%s\" is already used for\n\"%s\""),
                                name, data.conflict_item->description);
      g_free (name);

      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
          _("If you reassign the shortcut to \"%s\", the \"%s\" shortcut "
            "will be disabled."),
          item->description,
          data.conflict_item->description);

      gtk_dialog_add_button (GTK_DIALOG (dialog),
                             _("_Reassign"),
                             GTK_RESPONSE_ACCEPT);

      gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                       GTK_RESPONSE_ACCEPT);

      response = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      if (response == GTK_RESPONSE_ACCEPT)
        {
	  g_object_set (G_OBJECT (data.conflict_item), "binding", "", NULL);

          str = binding_name (keyval, keycode, mask, FALSE);
          g_object_set (G_OBJECT (item), "binding", str, NULL);

          g_free (str);
        }
      else
        {
          /* set it back to its previous value. */
        g_object_set (G_OBJECT (cell),
                      "accel-key", item->keyval,
                      "keycode", item->keycode,
                      "accel-mods", item->mask,
                      NULL);
        }

      return;
    }

  str = binding_name (keyval, keycode, mask, FALSE);
  g_object_set (G_OBJECT (item), "binding", str, NULL);

  g_free (str);
}

static void
accel_cleared_callback (GtkCellRendererText *cell,
                        const char          *path_string,
                        gpointer             data)
{
  GtkTreeView *view = (GtkTreeView *) data;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  CcKeyboardItem *item;
  GtkTreeIter iter;
  GtkTreeModel *model;

  model = gtk_tree_view_get_model (view);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);
  gtk_tree_model_get (model, &iter,
                      DETAIL_KEYENTRY_COLUMN, &item,
                      -1);

  /* sanity check */
  if (item == NULL)
    return;

  /* Unset the key */
  g_object_set (G_OBJECT (item), "binding", "", NULL);
}

static gchar *
find_free_settings_path ()
{
  char **used_names;
  char *dir = NULL;
  int i, num, n_names;

  used_names = g_settings_get_strv (binding_settings, "custom-keybindings");
  n_names = g_strv_length (used_names);

  for (num = 0; dir == NULL; num++)
    {
      char *tmp;
      gboolean found = FALSE;

      tmp = g_strdup_printf ("%s/custom%d/", CUSTOM_KEYS_BASENAME, num);
      for (i = 0; i < n_names && !found; i++)
        found = strcmp (used_names[i], tmp) == 0;

      if (!found)
        dir = tmp;
      else
        g_free (tmp);
    }

  return dir;
}

static void
add_custom_shortcut (GtkTreeView  *tree_view,
                     GtkTreeModel *model)
{
  CcKeyboardItem *item;
  GtkTreePath *path;
  gchar *settings_path;

  item = cc_keyboard_item_new (CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH);

  settings_path = find_free_settings_path ();
  cc_keyboard_item_load_from_gsettings_path (item, settings_path, TRUE);
  g_free (settings_path);

  item->model = model;

  if (edit_custom_shortcut (item) &&
      item->command && item->command[0])
    {
      GPtrArray *keys_array;
      GtkTreeIter iter;
      GHashTable *hash;
      GVariantBuilder builder;
      char **settings_paths;
      int i;

      hash = get_hash_for_group (BINDING_GROUP_USER);
      keys_array = g_hash_table_lookup (hash, CUSTOM_SHORTCUTS_ID);
      if (keys_array == NULL)
        {
          keys_array = g_ptr_array_new ();
          g_hash_table_insert (hash, g_strdup (CUSTOM_SHORTCUTS_ID), keys_array);
        }

      g_ptr_array_add (keys_array, item);

      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter, DETAIL_KEYENTRY_COLUMN, item, -1);

      settings_paths = g_settings_get_strv (binding_settings, "custom-keybindings");
      g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
      for (i = 0; settings_paths[i]; i++)
        g_variant_builder_add (&builder, "s", settings_paths[i]);
      g_variant_builder_add (&builder, "s", item->gsettings_path);
      g_settings_set_value (binding_settings, "custom-keybindings",
                            g_variant_builder_end (&builder));

      /* make the new shortcut visible */
      path = gtk_tree_model_get_path (model, &iter);
      gtk_tree_view_expand_to_path (tree_view, path);
      gtk_tree_view_scroll_to_cell (tree_view, path, NULL, FALSE, 0, 0);
      gtk_tree_path_free (path);
    }
  else
    {
      g_object_unref (item);
    }
}

static void
add_button_clicked (GtkWidget  *button,
                    GtkBuilder *builder)
{
  GtkTreeView *treeview;
  GtkTreeModel *model;
  GtkTreeModel *section_model;
  GtkTreeIter iter;
  gboolean found, cont;

  treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder,
                                                    "shortcut_treeview"));
  model = gtk_tree_view_get_model (treeview);

  /* Select the Custom Shortcuts section
   * before adding the shortcut itself */
  section_model = gtk_tree_view_get_model (GTK_TREE_VIEW (WID (builder, "section_treeview")));
  cont = gtk_tree_model_get_iter_first (section_model, &iter);
  found = FALSE;
  while (cont)
    {
      BindingGroupType group;

      gtk_tree_model_get (section_model, &iter,
                          SECTION_GROUP_COLUMN, &group,
                          -1);

      if (group == BINDING_GROUP_USER)
        {
          found = TRUE;
          break;
        }
      cont = gtk_tree_model_iter_next (section_model, &iter);
    }
  if (found)
    {
      GtkTreeSelection *selection;

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (WID (builder, "section_treeview")));
      gtk_tree_selection_select_iter (selection, &iter);
    }

  /* And add the shortcut */
  add_custom_shortcut (treeview, model);
}

static void
remove_button_clicked (GtkWidget  *button,
                       GtkBuilder *builder)
{
  GtkTreeView *treeview;
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  GtkTreeIter iter;

  treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder,
                                                    "shortcut_treeview"));
  model = gtk_tree_view_get_model (treeview);

  selection = gtk_tree_view_get_selection (treeview);
  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      remove_custom_shortcut (model, &iter);
    }
}

static int
section_sort_item  (GtkTreeModel *model,
                    GtkTreeIter  *a,
                    GtkTreeIter  *b,
                    gpointer      data)
{
  char *a_desc;
  int   a_group;
  char *b_desc;
  int   b_group;
  int   ret;

  gtk_tree_model_get (model, a,
                      SECTION_DESCRIPTION_COLUMN, &a_desc,
                      SECTION_GROUP_COLUMN, &a_group,
                      -1);
  gtk_tree_model_get (model, b,
                      SECTION_DESCRIPTION_COLUMN, &b_desc,
                      SECTION_GROUP_COLUMN, &b_group,
                      -1);

  if (a_group == b_group && a_desc && b_desc)
    ret = g_utf8_collate (a_desc, b_desc);
  else
    ret = a_group - b_group;

  g_free (a_desc);
  g_free (b_desc);

  return ret;
}

static gboolean
sections_separator_func (GtkTreeModel *model,
                         GtkTreeIter  *iter,
                         gpointer      data)
{
  BindingGroupType type;

  gtk_tree_model_get (model, iter, SECTION_GROUP_COLUMN, &type, -1);

  return type == BINDING_GROUP_SEPARATOR;
}

static void
xkb_options_combo_changed (GtkCellRendererCombo *combo,
                           gchar                *model_path,
                           GtkTreeIter          *model_iter,
                           gpointer              data)
{
  GtkTreeView *shortcut_treeview;
  GtkTreeModel *shortcut_model;
  GtkTreeIter shortcut_iter;
  GtkTreeSelection *selection;
  CcKeyboardOption *option;
  ShortcutType type;
  GtkBuilder *builder = data;

  shortcut_treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder, "shortcut_treeview"));
  selection = gtk_tree_view_get_selection (shortcut_treeview);
  if (!gtk_tree_selection_get_selected (selection, &shortcut_model, &shortcut_iter))
    return;

  gtk_tree_model_get (shortcut_model, &shortcut_iter,
                      DETAIL_KEYENTRY_COLUMN, &option,
                      DETAIL_TYPE_COLUMN, &type,
                      -1);

  if (type != SHORTCUT_TYPE_XKB_OPTION)
    return;

  cc_keyboard_option_set_selection (option, model_iter);
}

static gboolean
poke_xkb_option_row (GtkTreeModel *model,
                     GtkTreePath  *path,
                     GtkTreeIter  *iter,
                     gpointer      option)
{
  gpointer item;

  gtk_tree_model_get (model, iter,
                      DETAIL_KEYENTRY_COLUMN, &item,
                      -1);

  if (item != option)
    return FALSE;

  gtk_tree_model_row_changed (model, path, iter);
  return TRUE;
}

static void
xkb_option_changed (CcKeyboardOption *option,
                    gpointer          data)
{
  GtkTreeModel *model = data;

  gtk_tree_model_foreach (model, poke_xkb_option_row, option);
}

static void
setup_keyboard_options (GtkListStore *store)
{
  GList *l;

  for (l = cc_keyboard_option_get_all (); l; l = l->next)
    g_signal_connect (l->data, "changed",
                      G_CALLBACK (xkb_option_changed), store);
}

static void
setup_dialog (CcPanel *panel, GtkBuilder *builder)
{
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkWidget *widget;
  GtkTreeView *treeview;
  GtkTreeSelection *selection;
  CcShell *shell;
  GtkListStore *model;
  GtkTreeModelSort *sort_model;
  GtkStyleContext *context;

  gtk_widget_set_size_request (GTK_WIDGET (panel), -1, 400);

  /* Setup the section treeview */
  treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder, "section_treeview"));
  gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (treeview),
					sections_separator_func,
					panel,
					NULL);

  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("Section"),
                                                     renderer,
                                                     "text", SECTION_DESCRIPTION_COLUMN,
                                                     NULL);
  g_object_set (renderer,
                "width-chars", 20,
                "ellipsize", PANGO_ELLIPSIZE_END,
                NULL);

  gtk_tree_view_append_column (treeview, column);

  model = gtk_list_store_new (SECTION_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
  sort_model = GTK_TREE_MODEL_SORT (gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (model)));
  gtk_tree_view_set_model (treeview, GTK_TREE_MODEL (sort_model));
  g_object_unref (model);

  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (sort_model),
                                   SECTION_DESCRIPTION_COLUMN,
                                   section_sort_item,
                                   panel,
                                   NULL);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
                                        SECTION_DESCRIPTION_COLUMN,
                                        GTK_SORT_ASCENDING);
  g_object_unref (sort_model);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));

  gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);

  g_signal_connect (selection, "changed",
                    G_CALLBACK (section_selection_changed), builder);
  section_selection_changed (selection, builder);

  /* Setup the shortcut treeview */
  treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder,
                                                    "shortcut_treeview"));

  binding_settings = g_settings_new (BINDINGS_SCHEMA);

  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

  column = gtk_tree_view_column_new_with_attributes (NULL, renderer, NULL);
  gtk_tree_view_column_set_cell_data_func (column, renderer, description_set_func, NULL, NULL);
  gtk_tree_view_column_set_resizable (column, FALSE);
  gtk_tree_view_column_set_expand (column, TRUE);

  gtk_tree_view_append_column (treeview, column);

  renderer = (GtkCellRenderer *) g_object_new (GTK_TYPE_CELL_RENDERER_ACCEL,
                                               "accel-mode", GTK_CELL_RENDERER_ACCEL_MODE_OTHER,
                                               NULL);

  g_signal_connect (treeview, "button_press_event",
                    G_CALLBACK (start_editing_cb), renderer);
  g_signal_connect (treeview, "row-activated",
                    G_CALLBACK (start_editing_kb_cb), renderer);

  g_signal_connect (renderer, "accel_edited",
                    G_CALLBACK (accel_edited_callback),
                    treeview);
  g_signal_connect (renderer, "accel_cleared",
                    G_CALLBACK (accel_cleared_callback),
                    treeview);

  column = gtk_tree_view_column_new_with_attributes (NULL, renderer, NULL);
  gtk_tree_view_column_set_cell_data_func (column, renderer, accel_set_func, NULL, NULL);
  gtk_tree_view_column_set_resizable (column, FALSE);
  gtk_tree_view_column_set_expand (column, FALSE);

  renderer = (GtkCellRenderer *) g_object_new (GTK_TYPE_CELL_RENDERER_COMBO,
                                               "has-entry", FALSE,
                                               "text-column", XKB_OPTION_DESCRIPTION_COLUMN,
                                               "editable", TRUE,
                                               "ellipsize", PANGO_ELLIPSIZE_END,
                                               "width-chars", 25,
                                               NULL);
  g_signal_connect (renderer, "changed",
                    G_CALLBACK (xkb_options_combo_changed), builder);

  gtk_tree_view_column_pack_end (column, renderer, FALSE);

  gtk_tree_view_column_set_cell_data_func (column, renderer, accel_set_func, NULL, NULL);

  gtk_tree_view_append_column (treeview, column);

  model = gtk_list_store_new (DETAIL_N_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_INT);
  gtk_tree_view_set_model (treeview, GTK_TREE_MODEL (model));
  g_object_unref (model);

  setup_keyboard_options (model);

  widget = GTK_WIDGET (gtk_builder_get_object (builder, "actions_swindow"));
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_junction_sides (context, GTK_JUNCTION_BOTTOM);
  widget = GTK_WIDGET (gtk_builder_get_object (builder, "shortcut-toolbar"));
  context = gtk_widget_get_style_context (widget);
  gtk_style_context_set_junction_sides (context, GTK_JUNCTION_TOP);

  /* set up the dialog */
  shell = cc_panel_get_shell (CC_PANEL (panel));
  widget = cc_shell_get_toplevel (shell);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  g_signal_connect (selection, "changed",
                    G_CALLBACK (shortcut_selection_changed),
                    WID (builder, "remove-toolbutton"));

  /* setup the custom shortcut dialog */
  custom_shortcut_dialog = WID (builder,
                                "custom-shortcut-dialog");
  custom_shortcut_name_entry = WID (builder,
                                    "custom-shortcut-name-entry");
  custom_shortcut_command_entry = WID (builder,
                                       "custom-shortcut-command-entry");
  g_signal_connect (WID (builder, "add-toolbutton"),
                    "clicked", G_CALLBACK (add_button_clicked), builder);
  g_signal_connect (WID (builder, "remove-toolbutton"),
                    "clicked", G_CALLBACK (remove_button_clicked), builder);

  gtk_dialog_set_default_response (GTK_DIALOG (custom_shortcut_dialog),
                                   GTK_RESPONSE_OK);

  gtk_window_set_transient_for (GTK_WINDOW (custom_shortcut_dialog),
                                GTK_WINDOW (widget));

  gtk_window_set_resizable (GTK_WINDOW (custom_shortcut_dialog), FALSE);
}

static void
on_window_manager_change (const char *wm_name, CcPanel *panel)
{
  reload_sections (panel);
}

void
keyboard_shortcuts_init (CcPanel *panel, GtkBuilder *builder)
{
  g_object_set_data (G_OBJECT (panel), "builder", builder);
  wm_common_register_window_manager_change ((GFunc) on_window_manager_change,
                                            panel);
  setup_dialog (panel, builder);
  reload_sections (panel);
}

gboolean
keyboard_shortcuts_set_section (CcPanel *panel, const char *section)
{
  GtkBuilder *builder;
  GtkTreeModel *section_model;
  GtkTreeIter iter;
  gboolean found, cont;

  builder = g_object_get_data (G_OBJECT (panel), "builder");
  if (builder == NULL)
    {
      /* Remember the section name to be set later */
      g_object_set_data_full (G_OBJECT (panel), "section-to-set", g_strdup (section), g_free);
      return TRUE;
    }
  section_model = gtk_tree_view_get_model (GTK_TREE_VIEW (WID (builder, "section_treeview")));
  cont = gtk_tree_model_get_iter_first (section_model, &iter);
  found = FALSE;
  while (cont)
    {
      char *id;

      gtk_tree_model_get (section_model, &iter,
                          SECTION_ID_COLUMN, &id,
                          -1);

      if (g_strcmp0 (id, section) == 0)
        {
          found = TRUE;
          g_free (id);
          break;
        }
      g_free (id);
      cont = gtk_tree_model_iter_next (section_model, &iter);
    }
  if (found)
    {
      GtkTreeSelection *selection;

      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (WID (builder, "section_treeview")));
      gtk_tree_selection_select_iter (selection, &iter);
    }
  else
    {
      g_warning ("Could not find section '%s' to switch to.", section);
    }

  return found;
}

void
keyboard_shortcuts_dispose (CcPanel *panel)
{
  if (kb_system_sections != NULL)
    {
      g_hash_table_destroy (kb_system_sections);
      kb_system_sections = NULL;
    }
  if (kb_apps_sections != NULL)
    {
      g_hash_table_destroy (kb_apps_sections);
      kb_apps_sections = NULL;
    }
  if (kb_user_sections != NULL)
    {
      g_hash_table_destroy (kb_user_sections);
      kb_user_sections = NULL;
    }

  g_clear_object (&binding_settings);

  cc_keyboard_option_clear_all ();
}
