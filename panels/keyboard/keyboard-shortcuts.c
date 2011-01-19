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

#define MAX_CUSTOM_SHORTCUTS 1000
#define GCONF_BINDING_DIR "/desktop/gnome/keybindings"
#define WID(builder, name) (GTK_WIDGET (gtk_builder_get_object (builder, name)))

typedef struct {
  char *name;
  /* The group of keybindings (system or application) */
  char *group;
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
  BINDING_GROUP_SYSTEM,
  BINDING_GROUP_APP,
  BINDING_GROUP_USER,
};

enum
{
  DETAIL_DESCRIPTION_COLUMN,
  DETAIL_KEYENTRY_COLUMN,
  DETAIL_N_COLUMNS
};

enum
{
  SECTION_DESCRIPTION_COLUMN,
  SECTION_GROUP_COLUMN,
  SECTION_N_COLUMNS
};

static guint maybe_block_accels_id = 0;
static gboolean block_accels = FALSE;
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
          KeyEntry *key_entry;
          GConfClient *client;

          key_entry = g_ptr_array_index (keys, i);

          /* Remove GConf watches */
          client = gconf_client_get_default ();
          gconf_client_remove_dir (client, key_entry->gconf_key, NULL);
          gconf_client_notify_remove (client, key_entry->gconf_cnxn);
          if (key_entry->gconf_cnxn_desc != 0)
            gconf_client_notify_remove (client, key_entry->gconf_cnxn_desc);
          if (key_entry->gconf_cnxn_cmd != 0)
            gconf_client_notify_remove (client, key_entry->gconf_cnxn_cmd);

          g_object_unref (client);

          /* Free memory */
          g_free (key_entry->gconf_key);
          g_free (key_entry->description);
          g_free (key_entry->desc_gconf_key);
          g_free (key_entry->command);
          g_free (key_entry->cmd_gconf_key);

          g_free (key_entry);
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
                     guint                  *keycode,
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
                      DETAIL_KEYENTRY_COLUMN, &tmp_key_entry,
                      -1);

  if (key_entry == tmp_key_entry)
    {
      gtk_tree_model_row_changed (key_entry->model, path, iter);
      return TRUE;
    }
  return FALSE;
}

static void
keybinding_description_changed (GConfClient *client,
                                guint        cnxn_id,
                                GConfEntry  *entry,
                                gpointer     user_data)
{
  KeyEntry *key_entry;
  const gchar *key_value;

  key_entry = (KeyEntry *) user_data;
  key_value = entry->value ? gconf_value_get_string (entry->value) : NULL;

  g_free (key_entry->description);
  key_entry->description = key_value ? g_strdup (key_value) : NULL;
  key_entry->desc_editable = gconf_entry_get_is_writable (entry);

  /* update the model */
  gtk_tree_model_foreach (key_entry->model, keybinding_key_changed_foreach, key_entry);
}

static void
keybinding_key_changed (GConfClient *client,
                        guint        cnxn_id,
                        GConfEntry  *entry,
                        gpointer     user_data)
{
  KeyEntry *key_entry;
  const gchar *key_value;

  key_entry = (KeyEntry *) user_data;
  key_value = entry->value ? gconf_value_get_string (entry->value) : NULL;

  binding_from_string (key_value, &key_entry->keyval, &key_entry->keycode, &key_entry->mask);
  key_entry->editable = gconf_entry_get_is_writable (entry);

  /* update the model */
  gtk_tree_model_foreach (key_entry->model, keybinding_key_changed_foreach, key_entry);
}

static void
keybinding_command_changed (GConfClient *client,
                        guint        cnxn_id,
                        GConfEntry  *entry,
                        gpointer     user_data)
{
  KeyEntry *key_entry;
  const gchar *key_value;

  key_entry = (KeyEntry *) user_data;
  key_value = entry->value ? gconf_value_get_string (entry->value) : NULL;

  g_free (key_entry->command);
  key_entry->command = key_value ? g_strdup (key_value) : NULL;
  key_entry->cmd_editable = gconf_entry_get_is_writable (entry);

  /* update the model */
  gtk_tree_model_foreach (key_entry->model, keybinding_key_changed_foreach, key_entry);
}

static void
append_section (GtkBuilder         *builder,
                const gchar        *title,
                int                 group,
                const KeyListEntry *keys_list)
{
  GPtrArray *keys_array;
  GtkTreeModel *sort_model;
  GtkTreeModel *model;
  GConfClient *client;
  GtkTreeIter iter;
  gint i;

  client = gconf_client_get_default ();
  sort_model = gtk_tree_view_get_model (GTK_TREE_VIEW (gtk_builder_get_object (builder, "section_treeview")));
  model = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (sort_model));

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
          key_entry->gconf_cnxn_desc = gconf_client_notify_add (client, 
                                                                key_entry->desc_gconf_key,
                                                                (GConfClientNotifyFunc) &keybinding_description_changed,
                                                                key_entry, NULL, NULL);
        }
      if (keys_list[i].cmd_name != NULL)
        {
          key_entry->cmd_gconf_key =  g_strdup (keys_list[i].cmd_name);
          key_entry->cmd_editable = gconf_client_key_is_writable (client, key_entry->cmd_gconf_key, NULL);
          key_entry->gconf_cnxn_cmd = gconf_client_notify_add (client,
                                                               key_entry->cmd_gconf_key,
                                                               (GConfClientNotifyFunc) &keybinding_command_changed,
                                                               key_entry, NULL, NULL);
        }

      gconf_client_add_dir (client, key_string, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
      key_entry->gconf_cnxn = gconf_client_notify_add (client,
                                                       key_string,
                                                       (GConfClientNotifyFunc) &keybinding_key_changed,
                                                       key_entry, NULL, NULL);

      key_value = gconf_client_get_string (client, key_string, NULL);
      binding_from_string (key_value, &key_entry->keyval, &key_entry->keycode, &key_entry->mask);
      g_free (key_value);

      gconf_entry_free (entry);

      g_ptr_array_add (keys_array, key_entry);
    }

  g_object_unref (client);

  /* Add the keys to the hash table */
  if (keys_array->len > 0)
    {
      static gboolean have_sep = FALSE;

      g_hash_table_insert (kb_sections, g_strdup (title), keys_array);

      /* Append the section to the left tree view */
      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                          SECTION_DESCRIPTION_COLUMN, title,
                          SECTION_GROUP_COLUMN, group,
                          -1);
      if (!have_sep && group != BINDING_GROUP_SYSTEM)
        {
          have_sep = TRUE;
          /* Append the section to the left tree view */
          gtk_list_store_append (GTK_LIST_STORE (model), &iter);
          gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                              SECTION_DESCRIPTION_COLUMN, NULL,
                              SECTION_GROUP_COLUMN, BINDING_GROUP_SYSTEM,
                              -1);
        }
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
      if (group)
        {
          if (keylist->group)
            g_warning ("Duplicate group");
          g_free (keylist->group);
          keylist->group = g_strdup (group);
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
  if (keylist->group && strcmp (keylist->group, "system") == 0)
    group = BINDING_GROUP_SYSTEM;
  else
    group = BINDING_GROUP_APP;

  append_section (builder, title, group, keys);

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
      append_section (builder, _("Custom Shortcuts"), BINDING_GROUP_USER, keys);
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
  GtkTreeModel *sort_model;
  GtkTreeModel *section_model;
  GtkTreeModel *shortcut_model;

  sort_model = gtk_tree_view_get_model (GTK_TREE_VIEW (gtk_builder_get_object (builder, "section_treeview")));
  section_model = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (sort_model));

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
                      DETAIL_KEYENTRY_COLUMN, &key_entry,
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
                      DETAIL_KEYENTRY_COLUMN, &key_entry,
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

      gtk_tree_model_get (model, &iter, SECTION_DESCRIPTION_COLUMN, &description, -1);
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

          gtk_list_store_append (GTK_LIST_STORE (shortcut_model), &new_row);
          gtk_list_store_set (GTK_LIST_STORE (shortcut_model), &new_row,
                              DETAIL_DESCRIPTION_COLUMN, entry->description,
                              DETAIL_KEYENTRY_COLUMN, entry,
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
      gtk_tree_model_get (model, &iter, DETAIL_KEYENTRY_COLUMN, &key, -1);
      if (key && key->command != NULL && key->editable)
        can_remove = TRUE;
    }

  gtk_widget_set_sensitive (button, can_remove);
}

typedef struct
{
  GtkTreeView *tree_view;
  GtkTreePath *path;
  GtkTreeViewColumn *column;
} IdleData;

static gboolean
edit_custom_shortcut (KeyEntry *key)
{
  gint result;
  const gchar *text;
  gboolean ret;

  gtk_entry_set_text (GTK_ENTRY (custom_shortcut_name_entry), key->description ? key->description : "");
  gtk_widget_set_sensitive (custom_shortcut_name_entry, key->desc_editable);
  gtk_widget_grab_focus (custom_shortcut_name_entry);
  gtk_entry_set_text (GTK_ENTRY (custom_shortcut_command_entry), key->command ? key->command : "");
  gtk_widget_set_sensitive (custom_shortcut_command_entry, key->cmd_editable);

  gtk_window_present (GTK_WINDOW (custom_shortcut_dialog));
  result = gtk_dialog_run (GTK_DIALOG (custom_shortcut_dialog));
  switch (result)
    {
    case GTK_RESPONSE_OK:
      text = gtk_entry_get_text (GTK_ENTRY (custom_shortcut_name_entry));
      g_free (key->description);
      key->description = g_strdup (text);
      text = gtk_entry_get_text (GTK_ENTRY (custom_shortcut_command_entry));
      g_free (key->command);
      key->command = g_strdup (text);
      ret = TRUE;
      break;
    default:
      ret = FALSE;
      break;
    }

  gtk_widget_hide (custom_shortcut_dialog);

  return ret;
}

static gboolean
remove_custom_shortcut (GtkTreeModel *model, GtkTreeIter *iter)
{
  GtkTreeIter parent;
  GConfClient *client;
  gchar *base;
  KeyEntry *key;

  gtk_tree_model_get (model, iter,
                      DETAIL_KEYENTRY_COLUMN, &key,
                      -1);

  /* not a custom shortcut */
  if (key->command == NULL)
    return FALSE;

  client = gconf_client_get_default ();

  gconf_client_notify_remove (client, key->gconf_cnxn);
  if (key->gconf_cnxn_desc != 0)
    gconf_client_notify_remove (client, key->gconf_cnxn_desc);
  if (key->gconf_cnxn_cmd != 0)
    gconf_client_notify_remove (client, key->gconf_cnxn_cmd);

  base = g_path_get_dirname (key->gconf_key);
  gconf_client_recursive_unset (client, base, 0, NULL);
  g_free (base);
  /* suggest sync now so the unset directory actually gets dropped;
   * if we don't do this we may end up with 'zombie' shortcuts when
   * restarting the app */
  gconf_client_suggest_sync (client, NULL);
  g_object_unref (client);

  g_free (key->gconf_key);
  g_free (key->description);
  g_free (key->desc_gconf_key);
  g_free (key->command);
  g_free (key->cmd_gconf_key);
  g_free (key);

  gtk_tree_model_iter_parent (model, &parent, iter);
  gtk_tree_store_remove (GTK_TREE_STORE (model), iter);
  if (!gtk_tree_model_iter_has_child (model, &parent))
    gtk_tree_store_remove (GTK_TREE_STORE (model), &parent);

  return TRUE;
}

static void
update_custom_shortcut (GtkTreeModel *model, GtkTreeIter *iter)
{
  KeyEntry *key;

  gtk_tree_model_get (model, iter,
                      DETAIL_KEYENTRY_COLUMN, &key,
                      -1);

  edit_custom_shortcut (key);
  if (key->command == NULL || key->command[0] == '\0')
    {
      remove_custom_shortcut (model, iter);
    }
  else
    {
      GConfClient *client;

      gtk_tree_store_set (GTK_TREE_STORE (model), iter,
                          DETAIL_KEYENTRY_COLUMN, key, -1);
      client = gconf_client_get_default ();
      if (key->description != NULL)
        gconf_client_set_string (client, key->desc_gconf_key, key->description, NULL);
      else
        gconf_client_unset (client, key->desc_gconf_key, NULL);
      gconf_client_set_string (client, key->cmd_gconf_key, key->command, NULL);
      g_object_unref (client);
    }
}

static gboolean
real_start_editing_cb (IdleData *idle_data)
{
  gtk_widget_grab_focus (GTK_WIDGET (idle_data->tree_view));
  gtk_tree_view_set_cursor (idle_data->tree_view,
                            idle_data->path,
                            idle_data->column,
                            TRUE);
  gtk_tree_path_free (idle_data->path);
  g_free (idle_data);
  return FALSE;
}

static gboolean
start_editing_cb (GtkTreeView    *tree_view,
                  GdkEventButton *event,
                  gpointer        user_data)
{
  GtkTreePath *path;
  GtkTreeViewColumn *column;

  if (event->window != gtk_tree_view_get_bin_window (tree_view))
    return FALSE;

  if (gtk_tree_view_get_path_at_pos (tree_view,
                                     (gint) event->x,
                                     (gint) event->y,
                                     &path, &column,
                                     NULL, NULL))
    {
      IdleData *idle_data;
      GtkTreeModel *model;
      GtkTreeIter iter;
      KeyEntry *key;

      if (gtk_tree_path_get_depth (path) == 1)
        {
          gtk_tree_path_free (path);
          return FALSE;
        }

      model = gtk_tree_view_get_model (tree_view);
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_model_get (model, &iter,
                          DETAIL_KEYENTRY_COLUMN, &key,
                         -1);

      /* if only the accel can be edited on the selected row
       * always select the accel column */
      if (key->desc_editable &&
          column == gtk_tree_view_get_column (tree_view, 0))
        {
          gtk_widget_grab_focus (GTK_WIDGET (tree_view));
          gtk_tree_view_set_cursor (tree_view, path,
                                    gtk_tree_view_get_column (tree_view, 0),
                                    FALSE);
          update_custom_shortcut (model, &iter);
        }
      else
        {
          idle_data = g_new (IdleData, 1);
          idle_data->tree_view = tree_view;
          idle_data->path = path;
          idle_data->column = key->desc_editable ? column :
                              gtk_tree_view_get_column (tree_view, 1);
          g_idle_add ((GSourceFunc) real_start_editing_cb, idle_data);
          block_accels = TRUE;
        }
      g_signal_stop_emission_by_name (tree_view, "button_press_event");
    }
  return TRUE;
}

static void
start_editing_kb_cb (GtkTreeView *treeview,
                          GtkTreePath *path,
                          GtkTreeViewColumn *column,
                          gpointer user_data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  KeyEntry *key;

  model = gtk_tree_view_get_model (treeview);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get (model, &iter,
                      DETAIL_KEYENTRY_COLUMN, &key,
                      -1);

  if (key == NULL)
    {
      /* This is a section heading - expand or collapse */
      if (gtk_tree_view_row_expanded (treeview, path))
        gtk_tree_view_collapse_row (treeview, path);
      else
        gtk_tree_view_expand_row (treeview, path, FALSE);
      return;
    }

  /* if only the accel can be edited on the selected row
   * always select the accel column */
  if (key->desc_editable &&
      column == gtk_tree_view_get_column (treeview, 0))
    {
      gtk_widget_grab_focus (GTK_WIDGET (treeview));
      gtk_tree_view_set_cursor (treeview, path,
                                gtk_tree_view_get_column (treeview, 0),
                                FALSE);
      update_custom_shortcut (model, &iter);
    }
  else
    {
       gtk_widget_grab_focus (GTK_WIDGET (treeview));
       gtk_tree_view_set_cursor (treeview,
                                 path,
                                 gtk_tree_view_get_column (treeview, 1),
                                 TRUE);
    }
}

static void
description_edited_callback (GtkCellRendererText *renderer,
                             gchar               *path_string,
                             gchar               *new_text,
                             gpointer             user_data)
{
  GConfClient *client;
  GtkTreeView *view = GTK_TREE_VIEW (user_data);
  GtkTreeModel *model;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;
  KeyEntry *key_entry;

  model = gtk_tree_view_get_model (view);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);

  gtk_tree_model_get (model, &iter,
                      DETAIL_KEYENTRY_COLUMN, &key_entry,
                      -1);

  /* sanity check */
  if (key_entry == NULL || key_entry->desc_gconf_key == NULL)
    return;

  client = gconf_client_get_default ();
  if (!gconf_client_set_string (client, key_entry->desc_gconf_key, new_text, NULL))
    key_entry->desc_editable = FALSE;

  g_object_unref (client);
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
              EggVirtualModifierType  mask,
              gboolean                translate)
{
  if (keyval != 0 || keycode != 0)
    return translate ?
        egg_virtual_accelerator_label (keyval, keycode, mask) :
        egg_virtual_accelerator_name (keyval, keycode, mask);
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

static void
show_error (GtkWindow *parent,
            GError *err)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (parent,
                                   GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_OK,
                                   _("Error saving the new shortcut"));

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            "%s", err->message);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static gboolean
cb_check_for_uniqueness (GtkTreeModel *model,
                         GtkTreePath  *path,
                         GtkTreeIter  *iter,
                         KeyEntry *new_key)
{
  KeyEntry *element;

  gtk_tree_model_get (new_key->model, iter,
                      DETAIL_KEYENTRY_COLUMN, &element,
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
  new_key->desc_gconf_key = element->desc_gconf_key;
  new_key->desc_editable = element->desc_editable;
  return TRUE;
}

static void
accel_edited_callback (GtkCellRendererText   *cell,
                       const char            *path_string,
                       guint                  keyval,
                       EggVirtualModifierType mask,
                       guint                  keycode,
                       gpointer               data)
{
  GConfClient *client;
  GtkTreeView *view = (GtkTreeView *)data;
  GtkTreeModel *model;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;
  KeyEntry *key_entry, tmp_key;
  GError *err = NULL;
  char *str;

  block_accels = FALSE;

  model = gtk_tree_view_get_model (view);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);
  gtk_tree_model_get (model, &iter,
                      DETAIL_KEYENTRY_COLUMN, &key_entry,
                      -1);

  /* sanity check */
  if (key_entry == NULL)
    return;

  /* CapsLock isn't supported as a keybinding modifier, so keep it from confusing us */
  mask &= ~EGG_VIRTUAL_LOCK_MASK;

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

  /* Check for unmodified keys */
  if (tmp_key.mask == 0 && tmp_key.keycode != 0)
    {
      if ((tmp_key.keyval >= GDK_KEY_a && tmp_key.keyval <= GDK_KEY_z)
           || (tmp_key.keyval >= GDK_KEY_A && tmp_key.keyval <= GDK_KEY_Z)
           || (tmp_key.keyval >= GDK_KEY_0 && tmp_key.keyval <= GDK_KEY_9)
           || (tmp_key.keyval >= GDK_KEY_kana_fullstop && tmp_key.keyval <= GDK_KEY_semivoicedsound)
           || (tmp_key.keyval >= GDK_KEY_Arabic_comma && tmp_key.keyval <= GDK_KEY_Arabic_sukun)
           || (tmp_key.keyval >= GDK_KEY_Serbian_dje && tmp_key.keyval <= GDK_KEY_Cyrillic_HARDSIGN)
           || (tmp_key.keyval >= GDK_KEY_Greek_ALPHAaccent && tmp_key.keyval <= GDK_KEY_Greek_omega)
           || (tmp_key.keyval >= GDK_KEY_hebrew_doublelowline && tmp_key.keyval <= GDK_KEY_hebrew_taf)
           || (tmp_key.keyval >= GDK_KEY_Thai_kokai && tmp_key.keyval <= GDK_KEY_Thai_lekkao)
           || (tmp_key.keyval >= GDK_KEY_Hangul && tmp_key.keyval <= GDK_KEY_Hangul_Special)
           || (tmp_key.keyval >= GDK_KEY_Hangul_Kiyeog && tmp_key.keyval <= GDK_KEY_Hangul_J_YeorinHieuh)
           || keyval_is_forbidden (tmp_key.keyval)) {
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
        egg_cell_renderer_keys_set_accelerator
          (EGG_CELL_RENDERER_KEYS (cell),
           key_entry->keyval, key_entry->keycode, key_entry->mask);
        return;
      }
    }

  /* flag to see if the new accelerator was in use by something */
  if (!tmp_key.editable)
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
                                name, tmp_key.description ?
                                tmp_key.description : tmp_key.gconf_key);
      g_free (name);

      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
          _("If you reassign the shortcut to \"%s\", the \"%s\" shortcut "
            "will be disabled."),
          key_entry->description ?
          key_entry->description : key_entry->gconf_key,
          tmp_key.description ?
          tmp_key.description : tmp_key.gconf_key);

      gtk_dialog_add_button (GTK_DIALOG (dialog),
                             _("_Reassign"),
                             GTK_RESPONSE_ACCEPT);

      gtk_dialog_set_default_response (GTK_DIALOG (dialog),
                                       GTK_RESPONSE_ACCEPT);

      response = gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);

      if (response == GTK_RESPONSE_ACCEPT)
        {
          GConfClient *client;

          client = gconf_client_get_default ();

          gconf_client_set_string (client,
                                   tmp_key.gconf_key,
                                   "", &err);

          if (err != NULL)
            {
               show_error (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
                           err);
               g_error_free (err);
               g_object_unref (client);
               return;
            }

          str = binding_name (keyval, keycode, mask, FALSE);
          gconf_client_set_string (client,
                                   key_entry->gconf_key,
                                   str, &err);

          if (err != NULL)
            {
               show_error (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
                           err);
               g_error_free (err);

               /* reset the previous shortcut */
               gconf_client_set_string (client,
                                        tmp_key.gconf_key,
                                        str, NULL);
            }

          g_free (str);
          g_object_unref (client);
        }
      else
        {
          /* set it back to its previous value. */
          egg_cell_renderer_keys_set_accelerator (EGG_CELL_RENDERER_KEYS (cell),
                                                  key_entry->keyval,
                                                  key_entry->keycode,
                                                  key_entry->mask);
        }

      return;
    }

  str = binding_name (keyval, keycode, mask, FALSE);

  client = gconf_client_get_default ();
  gconf_client_set_string (client,
                           key_entry->gconf_key,
                           str,
                           &err);
  g_free (str);
  g_object_unref (client);

  if (err != NULL)
    {
      show_error (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))), err);
      g_error_free (err);
      key_entry->editable = FALSE;
    }
}

static void
accel_cleared_callback (GtkCellRendererText *cell,
                        const char          *path_string,
                        gpointer             data)
{
  GConfClient *client;
  GtkTreeView *view = (GtkTreeView *) data;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  KeyEntry *key_entry;
  GtkTreeIter iter;
  GError *err = NULL;
  GtkTreeModel *model;

  block_accels = FALSE;

  model = gtk_tree_view_get_model (view);
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_path_free (path);
  gtk_tree_model_get (model, &iter,
                      DETAIL_KEYENTRY_COLUMN, &key_entry,
                      -1);

  /* sanity check */
  if (key_entry == NULL)
    return;

  /* Unset the key */
  client = gconf_client_get_default();
  gconf_client_set_string (client,
                           key_entry->gconf_key,
                           "",
                           &err);
  g_object_unref (client);

  if (err != NULL)
    {
      GtkWidget *dialog;

      dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (view))),
                                       GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_WARNING,
                                       GTK_BUTTONS_OK,
                                       _("Error unsetting accelerator in configuration database: %s"),
                                       err->message);
      gtk_dialog_run (GTK_DIALOG (dialog));

      gtk_widget_destroy (dialog);
      g_error_free (err);
      key_entry->editable = FALSE;
    }
}

static void
key_entry_controlling_key_changed (GConfClient *client,
                                   guint        cnxn_id,
                                   GConfEntry  *entry,
                                   gpointer     user_data)
{
  reload_sections (user_data);
}

/* this handler is used to keep accels from activating while the user
 * is assigning a new shortcut so that he won't accidentally trigger one
 * of the widgets */
static gboolean
maybe_block_accels (GtkWidget *widget,
                    GdkEventKey *event,
                    gpointer user_data)
{
  if (block_accels)
  {
    return gtk_window_propagate_key_event (GTK_WINDOW (widget), event);
  }
  return FALSE;
}

static gchar *
find_free_gconf_key (GError **error)
{
  GConfClient *client;

  gchar *dir;
  int i;

  client = gconf_client_get_default ();

  for (i = 0; i < MAX_CUSTOM_SHORTCUTS; i++)
    {
      dir = g_strdup_printf ("%s/custom%d", GCONF_BINDING_DIR, i);
      if (!gconf_client_dir_exists (client, dir, NULL))
        break;
      g_free (dir);
    }

  if (i == MAX_CUSTOM_SHORTCUTS)
    {
      dir = NULL;
      g_set_error_literal (error,
                           g_quark_from_string ("Keyboard Shortcuts"),
                           0,
                           _("Too many custom shortcuts"));
    }

  g_object_unref (client);

  return dir;
}

static void
add_custom_shortcut (GtkTreeView  *tree_view,
                     GtkTreeModel *model)
{
  KeyEntry *key_entry;
  GtkTreePath *path;
  gchar *dir;
  GConfClient *client;
  GError *error;

  error = NULL;
  dir = find_free_gconf_key (&error);
  if (dir == NULL)
    {
      show_error (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (tree_view))), error);

      g_error_free (error);
      return;
    }

  key_entry = g_new0 (KeyEntry, 1);
  key_entry->gconf_key = g_strconcat (dir, "/binding", NULL);
  key_entry->editable = TRUE;
  key_entry->model = model;
  key_entry->desc_gconf_key = g_strconcat (dir, "/name", NULL);
  key_entry->description = g_strdup ("");
  key_entry->desc_editable = TRUE;
  key_entry->cmd_gconf_key = g_strconcat (dir, "/action", NULL);
  key_entry->command = g_strdup ("");
  key_entry->cmd_editable = TRUE;
  g_free (dir);

  if (edit_custom_shortcut (key_entry) &&
      key_entry->command && key_entry->command[0])
    {
      GPtrArray *keys_array;
      GtkTreeIter iter;

      keys_array = g_hash_table_lookup (kb_sections, _("Custom Shortcuts"));
      if (keys_array == NULL)
        {
          keys_array = g_ptr_array_new ();
          g_hash_table_insert (kb_sections, g_strdup (_("Custom Shortcuts")), keys_array);
        }

      g_ptr_array_add (keys_array, key_entry);

      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter, DETAIL_KEYENTRY_COLUMN, key_entry, -1);

      /* store in gconf */
      client = gconf_client_get_default ();
      gconf_client_set_string (client, key_entry->gconf_key, "", NULL);
      gconf_client_set_string (client, key_entry->desc_gconf_key, key_entry->description, NULL);
      gconf_client_set_string (client, key_entry->cmd_gconf_key, key_entry->command, NULL);

      /* add gconf watches */
      key_entry->gconf_cnxn_desc = gconf_client_notify_add (client,
                                                            key_entry->desc_gconf_key,
                                                            (GConfClientNotifyFunc) &keybinding_description_changed,
                                                            key_entry, NULL, NULL);
      key_entry->gconf_cnxn_cmd = gconf_client_notify_add (client,
                                                           key_entry->cmd_gconf_key,
                                                           (GConfClientNotifyFunc) &keybinding_command_changed,
                                                           key_entry, NULL, NULL);
      key_entry->gconf_cnxn = gconf_client_notify_add (client,
                                                       key_entry->gconf_key,
                                                       (GConfClientNotifyFunc) &keybinding_key_changed,
                                                       key_entry, NULL, NULL);


      g_object_unref (client);

      /* make the new shortcut visible */
      path = gtk_tree_model_get_path (model, &iter);
      gtk_tree_view_expand_to_path (tree_view, path);
      gtk_tree_view_scroll_to_cell (tree_view, path, NULL, FALSE, 0, 0);
      gtk_tree_path_free (path);
    }
  else
    {
      g_free (key_entry->gconf_key);
      g_free (key_entry->description);
      g_free (key_entry->desc_gconf_key);
      g_free (key_entry->command);
      g_free (key_entry->cmd_gconf_key);
      g_free (key_entry);
    }
}

static void
add_button_clicked (GtkWidget  *button,
                    GtkBuilder *builder)
{
  GtkTreeView *treeview;
  GtkTreeModel *model;

  treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder,
                                                    "shortcut_treeview"));
  model = gtk_tree_view_get_model (treeview);

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
keyentry_sort_func (GtkTreeModel *model,
                    GtkTreeIter  *a,
                    GtkTreeIter  *b,
                    gpointer      user_data)
{
  KeyEntry *key_entry_a;
  KeyEntry *key_entry_b;
  int retval;

  key_entry_a = NULL;
  gtk_tree_model_get (model, a,
                      DETAIL_KEYENTRY_COLUMN, &key_entry_a,
                      -1);

  key_entry_b = NULL;
  gtk_tree_model_get (model, b,
                      DETAIL_KEYENTRY_COLUMN, &key_entry_b,
                      -1);

  if (key_entry_a && key_entry_b)
    {
      if ((key_entry_a->keyval || key_entry_a->keycode) &&
          (key_entry_b->keyval || key_entry_b->keycode))
        {
          gchar *name_a, *name_b;

          name_a = binding_name (key_entry_a->keyval,
                                 key_entry_a->keycode,
                                 key_entry_a->mask,
                                 TRUE);

          name_b = binding_name (key_entry_b->keyval,
                                 key_entry_b->keycode,
                                 key_entry_b->mask,
                                 TRUE);

          retval = g_utf8_collate (name_a, name_b);

          g_free (name_a);
          g_free (name_b);
        }
      else if (key_entry_a->keyval || key_entry_a->keycode)
        retval = -1;
      else if (key_entry_b->keyval || key_entry_b->keycode)
        retval = 1;
      else
        retval = 0;
    }
  else if (key_entry_a)
    retval = -1;
  else if (key_entry_b)
    retval = 1;
  else
    retval = 0;

  return retval;
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

  ret = 0;

  gtk_tree_model_get (model, a,
                      SECTION_DESCRIPTION_COLUMN, &a_desc,
                      SECTION_GROUP_COLUMN, &a_group,
                      -1);
  gtk_tree_model_get (model, b,
                      SECTION_DESCRIPTION_COLUMN, &b_desc,
                      SECTION_GROUP_COLUMN, &b_group,
                      -1);

  if (a_group == b_group)
    {
      /* separators go after the section */
      if (a_desc == NULL)
        ret = 1;
      else if (b_desc == NULL)
        ret = -1;
      else
        ret = g_utf8_collate (a_desc, b_desc);
    }
  else
    {
      if (a_group < b_group)
        ret = -1;
      else
        ret = 1;
    }

  return ret;
}

static gboolean
sections_separator_func (GtkTreeModel *model,
                         GtkTreeIter  *iter,
                         gpointer      data)
{
  char    *description;
  gboolean is_separator;

  description = NULL;
  is_separator = FALSE;

  gtk_tree_model_get (model, iter, SECTION_DESCRIPTION_COLUMN, &description, -1);
  if (description == NULL)
    is_separator = TRUE;
  g_free (description);

  return is_separator;
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
  GtkTreeModelSort *sort_model;

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
  gtk_tree_view_append_column (treeview, column);

  model = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
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
  g_signal_connect (selection, "changed",
                    G_CALLBACK (section_selection_changed), builder);

  /* Setup the shortcut treeview */
  treeview = GTK_TREE_VIEW (gtk_builder_get_object (builder,
                                                    "shortcut_treeview"));

  client = gconf_client_get_default ();

  g_signal_connect (treeview, "button_press_event",
                    G_CALLBACK (start_editing_cb), builder);
  g_signal_connect (treeview, "row-activated",
                    G_CALLBACK (start_editing_kb_cb), NULL);

  renderer = gtk_cell_renderer_text_new ();

  g_signal_connect (renderer, "edited",
                    G_CALLBACK (description_edited_callback),
                    treeview);

  column = gtk_tree_view_column_new_with_attributes (_("Action"),
                                                     renderer,
                                                     "text", DETAIL_DESCRIPTION_COLUMN,
                                                     NULL);
  gtk_tree_view_column_set_cell_data_func (column, renderer, description_set_func, NULL, NULL);
  gtk_tree_view_column_set_resizable (column, FALSE);

  gtk_tree_view_append_column (treeview, column);
  gtk_tree_view_column_set_sort_column_id (column, DETAIL_DESCRIPTION_COLUMN);

  renderer = (GtkCellRenderer *) g_object_new (EGG_TYPE_CELL_RENDERER_KEYS,
                                               "accel_mode", EGG_CELL_RENDERER_KEYS_MODE_X,
                                               NULL);

  g_signal_connect (renderer, "accel_edited",
                    G_CALLBACK (accel_edited_callback),
                    treeview);

  g_signal_connect (renderer, "accel_cleared",
                    G_CALLBACK (accel_cleared_callback),
                    treeview);

  column = gtk_tree_view_column_new_with_attributes (_("Shortcut"), renderer, NULL);
  gtk_tree_view_column_set_cell_data_func (column, renderer, accel_set_func, NULL, NULL);
  gtk_tree_view_column_set_resizable (column, FALSE);

  gtk_tree_view_append_column (treeview, column);
  gtk_tree_view_column_set_sort_column_id (column, DETAIL_KEYENTRY_COLUMN);

  gconf_client_add_dir (client, GCONF_BINDING_DIR, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  gconf_client_add_dir (client, "/apps/metacity/general", GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  gconf_client_notify_add (client,
                           "/apps/metacity/general/num_workspaces",
                           (GConfClientNotifyFunc) key_entry_controlling_key_changed,
                           builder, NULL, NULL);

  model = gtk_list_store_new (DETAIL_N_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER);
  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (model),
                                   DETAIL_KEYENTRY_COLUMN,
                                   keyentry_sort_func,
                                   NULL, NULL);
  gtk_tree_view_set_model (treeview, GTK_TREE_MODEL (model));
  g_object_unref (model);

  /* set up the dialog */
  shell = cc_panel_get_shell (CC_PANEL (panel));
  widget = cc_shell_get_toplevel (shell);

  maybe_block_accels_id = g_signal_connect (widget, "key_press_event",
                                            G_CALLBACK (maybe_block_accels), NULL);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
  g_signal_connect (selection, "changed",
                    G_CALLBACK (shortcut_selection_changed),
                    WID (builder, "remove-toolbutton"));

  allowed_keys = gconf_client_get_list (client,
                                        GCONF_BINDING_DIR "/allowed_keys",
                                        GCONF_VALUE_STRING,
                                        NULL);
  if (allowed_keys != NULL)
    {
      g_slist_foreach (allowed_keys, (GFunc)g_free, NULL);
      g_slist_free (allowed_keys);
      gtk_widget_set_sensitive (WID (builder, "add-toolbutton"),
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
  g_signal_connect (WID (builder, "add-toolbutton"),
                    "clicked", G_CALLBACK (add_button_clicked), builder);
  g_signal_connect (WID (builder, "remove-toolbutton"),
                    "clicked", G_CALLBACK (remove_button_clicked), builder);

  gtk_dialog_set_default_response (GTK_DIALOG (custom_shortcut_dialog),
                                   GTK_RESPONSE_OK);

  gtk_window_set_transient_for (GTK_WINDOW (custom_shortcut_dialog),
                                GTK_WINDOW (widget));
}

static void
on_window_manager_change (const char *wm_name, GtkBuilder *builder)
{
  reload_sections (builder);
}

void
keyboard_shortcuts_init (CcPanel *panel, GtkBuilder *builder)
{
  kb_sections = g_hash_table_new_full (g_str_hash, g_str_equal,
                                       g_free, (GDestroyNotify) free_key_array);

  wm_common_register_window_manager_change ((GFunc) on_window_manager_change,
                                            builder);
  setup_dialog (panel, builder);
  reload_sections (builder);
}

void
keyboard_shortcuts_dispose (CcPanel *panel)
{
  if (maybe_block_accels_id != 0)
    {
      CcShell *shell;
      GtkWidget *toplevel;

      shell = cc_panel_get_shell (CC_PANEL (panel));
      toplevel = cc_shell_get_toplevel (shell);

      g_signal_handler_disconnect (toplevel, maybe_block_accels_id);
      maybe_block_accels_id = 0;

      if (kb_sections != NULL)
        g_hash_table_destroy (kb_sections);
    }
}
