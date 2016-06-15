/*
 * Copyright (C) 2010 Intel, Inc
 * Copyright (C) 2016 Endless, Inc
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Thomas Wood <thomas.wood@intel.com>
 *         Georges Basile Stavracas Neto <gbsneto@gnome.org>
 *
 */

#include <glib/gi18n.h>

#include "cc-keyboard-item.h"
#include "cc-keyboard-option.h"
#include "cc-keyboard-panel.h"
#include "cc-keyboard-resources.h"

#include "keyboard-shortcuts.h"
#include "wm-common.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#define BINDINGS_SCHEMA       "org.gnome.settings-daemon.plugins.media-keys"
#define CUSTOM_SHORTCUTS_ID   "custom"

struct _CcKeyboardPanel
{
  CcPanel             parent;

  /* Treeviews */
  GtkListStore       *sections_store;
  GtkTreeModel       *sections_model;
  GtkWidget          *shortcut_treeview;

  /* Toolbar widgets */
  GtkWidget          *add_toolbutton;
  GtkWidget          *remove_toolbutton;
  GtkWidget          *shortcut_toolbar;

  /* Custom shortcut dialog */
  GtkWidget          *custom_shortcut_command_entry;
  GtkWidget          *custom_shortcut_dialog;
  GtkWidget          *custom_shortcut_name_entry;
  GtkWidget          *custom_shortcut_ok_button;

  GHashTable         *kb_system_sections;
  GHashTable         *kb_apps_sections;
  GHashTable         *kb_user_sections;

  GSettings          *binding_settings;

  GRegex             *pictures_regex;

  gpointer            wm_changed_id;
};

CC_PANEL_REGISTER (CcKeyboardPanel, cc_keyboard_panel)

enum {
  PROP_0,
  PROP_PARAMETERS
};

static GHashTable*
get_hash_for_group (CcKeyboardPanel  *self,
                    BindingGroupType  group)
{
  GHashTable *hash;

  switch (group)
    {
    case BINDING_GROUP_SYSTEM:
      hash = self->kb_system_sections;
      break;
    case BINDING_GROUP_APPS:
      hash = self->kb_apps_sections;
      break;
    case BINDING_GROUP_USER:
      hash = self->kb_user_sections;
      break;
    default:
      hash = NULL;
    }

  return hash;
}

static gboolean
have_key_for_group (CcKeyboardPanel *self,
                    int              group,
                    const gchar     *name)
{
  GHashTableIter iter;
  GPtrArray *keys;
  gint i;

  g_hash_table_iter_init (&iter, get_hash_for_group (self, group));
  while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &keys))
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

static char*
binding_name (guint                   keyval,
              guint                   keycode,
              GdkModifierType         mask,
              gboolean                translate)
{
  if (keyval != 0 || keycode != 0)
    {
      return translate ? gtk_accelerator_get_label_with_keycode (NULL, keyval, keycode, mask) :
                         gtk_accelerator_name_with_keycode (NULL, keyval, keycode, mask);
    }
  else
    {
      return g_strdup (translate ? _("Disabled") : NULL);
    }
}


static gboolean
keybinding_key_changed_foreach (GtkTreeModel   *model,
                                GtkTreePath    *path,
                                GtkTreeIter    *iter,
                                CcKeyboardItem *item)
{
  CcKeyboardItem *tmp_item;

  gtk_tree_model_get (item->model,
                      iter,
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
  gtk_tree_model_foreach (item->model,
                          (GtkTreeModelForeachFunc) keybinding_key_changed_foreach,
                          item);
}

static void
append_section (CcKeyboardPanel    *self,
                const gchar        *title,
                const gchar        *id,
                BindingGroupType    group,
                const KeyListEntry *keys_list)
{
  GtkTreeModel *shortcut_model;
  GtkTreeIter iter;
  GHashTable *reverse_items;
  GHashTable *hash;
  GPtrArray *keys_array;
  gboolean is_new;
  gint i;

  hash = get_hash_for_group (self, group);

  if (!hash)
    return;

  shortcut_model = gtk_tree_view_get_model (GTK_TREE_VIEW (self->shortcut_treeview));

  /* Add all CcKeyboardItems for this section */
  is_new = FALSE;
  keys_array = g_hash_table_lookup (hash, id);
  if (keys_array == NULL)
    {
      keys_array = g_ptr_array_new ();
      is_new = TRUE;
    }

  reverse_items = g_hash_table_new (g_str_hash, g_str_equal);

  for (i = 0; keys_list != NULL && keys_list[i].name != NULL; i++)
    {
      CcKeyboardItem *item;
      gboolean ret;

      if (have_key_for_group (self, group, keys_list[i].name))
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
          if (ret && keys_list[i].reverse_entry != NULL)
            {
              CcKeyboardItem *reverse_item;
              reverse_item = g_hash_table_lookup (reverse_items,
                                                  keys_list[i].reverse_entry);
              if (reverse_item != NULL)
                {
                  cc_keyboard_item_add_reverse_item (item,
                                                     reverse_item,
                                                     keys_list[i].is_reversed);
                }
              else
                {
                  g_hash_table_insert (reverse_items,
                                       keys_list[i].name,
                                       item);
                }
            }
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

      cc_keyboard_item_set_hidden (item, keys_list[i].hidden);
      item->model = shortcut_model;

      g_signal_connect (G_OBJECT (item),
                        "notify",
                        G_CALLBACK (item_changed),
                        NULL);

      g_ptr_array_add (keys_array, item);
    }

  g_hash_table_destroy (reverse_items);

  /* Add the keys to the hash table */
  if (is_new)
    {
      g_hash_table_insert (hash, g_strdup (id), keys_array);

      /* Append the section to the left tree view */
      gtk_list_store_append (GTK_LIST_STORE (self->sections_store), &iter);
      gtk_list_store_set (GTK_LIST_STORE (self->sections_store),
                          &iter,
                          SECTION_DESCRIPTION_COLUMN, title,
                          SECTION_ID_COLUMN, id,
                          SECTION_GROUP_COLUMN, group,
                          -1);
    }
}

static void
append_sections_from_file (CcKeyboardPanel  *self,
                           const gchar      *path,
                           const char       *datadir,
                           gchar           **wm_keybindings)
{
  KeyList *keylist;
  KeyListEntry *keys;
  KeyListEntry key = { 0, 0, 0, 0, 0, 0, 0 };
  const char *title;
  int group;
  guint i;

  keylist = parse_keylist_from_file (path);

  if (keylist == NULL)
    return;

#define const_strv(s) ((const gchar* const*) s)

  /* If there's no keys to add, or the settings apply to a window manager
   * that's not the one we're running */
  if (keylist->entries->len == 0 ||
      (keylist->wm_name != NULL && !g_strv_contains (const_strv (wm_keybindings), keylist->wm_name)) ||
      keylist->name == NULL)
    {
      g_free (keylist->name);
      g_free (keylist->package);
      g_free (keylist->wm_name);
      g_array_free (keylist->entries, TRUE);
      g_free (keylist);
      return;
    }

#undef const_strv

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

  append_section (self, title, keylist->name, group, keys);

  g_free (keylist->name);
  g_free (keylist->package);
  g_free (keylist->wm_name);
  g_free (keylist->schema);
  g_free (keylist->group);

  for (i = 0; keys[i].name != NULL; i++)
    {
      KeyListEntry *entry = &keys[i];
      g_free (entry->schema);
      g_free (entry->description);
      g_free (entry->name);
      g_free (entry->reverse_entry);
    }

  g_free (keylist);
  g_free (keys);
}

static void
append_sections_from_gsettings (CcKeyboardPanel *self)
{
  char **custom_paths;
  GArray *entries;
  KeyListEntry key = { 0, 0, 0, 0, 0, 0, 0 };
  int i;

  /* load custom shortcuts from GSettings */
  entries = g_array_new (FALSE, TRUE, sizeof (KeyListEntry));

  custom_paths = g_settings_get_strv (self->binding_settings, "custom-keybindings");
  for (i = 0; custom_paths[i]; i++)
    {
      key.name = g_strdup (custom_paths[i]);
      if (!have_key_for_group (self, BINDING_GROUP_USER, key.name))
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
      append_section (self, _("Custom Shortcuts"), CUSTOM_SHORTCUTS_ID, BINDING_GROUP_USER, keys);
      for (i = 0; i < entries->len; ++i)
        {
          g_free (keys[i].name);
        }
    }
  else
    {
      append_section (self, _("Custom Shortcuts"), CUSTOM_SHORTCUTS_ID, BINDING_GROUP_USER, NULL);
    }

  g_array_free (entries, TRUE);
}

static void
reload_sections (CcKeyboardPanel *self)
{
  GtkTreeModel *shortcut_model;
  GtkTreeIter iter;
  GHashTable *loaded_files;
  GDir *dir;
  gchar *default_wm_keybindings[] = { "Mutter", "GNOME Shell", NULL };
  gchar **wm_keybindings;
  const gchar * const * data_dirs;
  guint i;

  shortcut_model = gtk_tree_view_get_model (GTK_TREE_VIEW (self->shortcut_treeview));
  /* FIXME: get current selection and keep it after refreshing */

  /* Clear previous models and hash tables */
  gtk_list_store_clear (GTK_LIST_STORE (self->sections_store));
  gtk_list_store_clear (GTK_LIST_STORE (shortcut_model));

  g_clear_pointer (&self->kb_system_sections, g_hash_table_destroy);
  self->kb_system_sections = g_hash_table_new_full (g_str_hash,
                                                    g_str_equal,
                                                    g_free,
                                                    (GDestroyNotify) free_key_array);

  g_clear_pointer (&self->kb_apps_sections, g_hash_table_destroy);
  self->kb_apps_sections = g_hash_table_new_full (g_str_hash,
                                                  g_str_equal,
                                                  g_free,
                                                  (GDestroyNotify) free_key_array);

  g_clear_pointer (&self->kb_user_sections, g_hash_table_destroy);
  self->kb_user_sections = g_hash_table_new_full (g_str_hash,
                                                  g_str_equal,
                                                  g_free,
                                                  (GDestroyNotify) free_key_array);

  /* Load WM keybindings */
#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    wm_keybindings = wm_common_get_current_keybindings ();
  else
#endif
    wm_keybindings = g_strdupv (default_wm_keybindings);

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
          append_sections_from_file (self, path, data_dirs[i], wm_keybindings);
          g_free (path);
        }
      g_free (dir_path);
      g_dir_close (dir);
    }

  g_hash_table_destroy (loaded_files);
  g_strfreev (wm_keybindings);

  /* Add a separator */
  gtk_list_store_append (GTK_LIST_STORE (self->sections_store), &iter);
  gtk_list_store_set (GTK_LIST_STORE (self->sections_store),
                      &iter,
                      SECTION_DESCRIPTION_COLUMN, NULL,
                      SECTION_GROUP_COLUMN, BINDING_GROUP_SEPARATOR,
                      -1);

  /* Load custom keybindings */
  append_sections_from_gsettings (self);
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

static void
add_shortcuts (CcKeyboardPanel *self)
{
  GtkTreeModel *shortcuts;
  GtkTreeIter sections_iter;
  gboolean can_continue;

  shortcuts = gtk_tree_view_get_model (GTK_TREE_VIEW (self->shortcut_treeview));
  can_continue = gtk_tree_model_get_iter_first (self->sections_model, &sections_iter);

  while (can_continue)
    {
      BindingGroupType group;
      GPtrArray *keys;
      gchar *id, *title;
      gint i;

      gtk_tree_model_get (self->sections_model,
                          &sections_iter,
                          SECTION_DESCRIPTION_COLUMN, &title,
                          SECTION_GROUP_COLUMN, &group,
                          SECTION_ID_COLUMN, &id,
                          -1);

      /* Ignore separators */
      if (group == BINDING_GROUP_SEPARATOR)
        {
          can_continue = gtk_tree_model_iter_next (self->sections_model, &sections_iter);
          continue;
        }

      keys = g_hash_table_lookup (get_hash_for_group (self, group), id);

      for (i = 0; i < keys->len; i++)
        {
          CcKeyboardItem *item = g_ptr_array_index (keys, i);

          if (!cc_keyboard_item_is_hidden (item))
            {
              GtkTreeIter new_row;

              gtk_list_store_append (GTK_LIST_STORE (shortcuts), &new_row);
              gtk_list_store_set (GTK_LIST_STORE (shortcuts),
                                  &new_row,
                                  DETAIL_DESCRIPTION_COLUMN, item->description,
                                  DETAIL_KEYENTRY_COLUMN, item,
                                  DETAIL_TYPE_COLUMN, SHORTCUT_TYPE_KEY_ENTRY,
                                  -1);
            }
        }

      can_continue = gtk_tree_model_iter_next (self->sections_model, &sections_iter);
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
shortcut_selection_changed (GtkTreeSelection *selection,
                            GtkWidget        *button)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean can_remove;

  can_remove = FALSE;

  if (gtk_tree_selection_get_selected (selection, &model, &iter))
    {
      CcKeyboardItem *item;
      ShortcutType type;

      gtk_tree_model_get (model, &iter,
                          DETAIL_KEYENTRY_COLUMN, &item,
                          DETAIL_TYPE_COLUMN, &type,
                          -1);

      if (type == SHORTCUT_TYPE_KEY_ENTRY &&
          item &&
          item->command != NULL &&
          item->editable)
        {
          can_remove = TRUE;
        }
    }

  gtk_widget_set_sensitive (button, can_remove);
}


static gboolean
edit_custom_shortcut (CcKeyboardPanel *self,
                      CcKeyboardItem  *item)
{
  gint result;
  gboolean ret;
  GSettings *settings;

  settings = g_settings_new_with_path (item->schema, item->gsettings_path);

  g_settings_bind (settings, "name",
                   G_OBJECT (self->custom_shortcut_name_entry), "text",
                   G_SETTINGS_BIND_DEFAULT);
  gtk_widget_grab_focus (self->custom_shortcut_name_entry);

  g_settings_bind (settings, "command",
                   G_OBJECT (self->custom_shortcut_command_entry), "text",
                   G_SETTINGS_BIND_DEFAULT);

  g_settings_delay (settings);

  gtk_widget_set_sensitive (self->custom_shortcut_name_entry, item->desc_editable);
  gtk_widget_set_sensitive (self->custom_shortcut_command_entry, item->cmd_editable);
  gtk_window_present (GTK_WINDOW (self->custom_shortcut_dialog));

  result = gtk_dialog_run (GTK_DIALOG (self->custom_shortcut_dialog));
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

  g_settings_unbind (G_OBJECT (self->custom_shortcut_name_entry), "text");
  g_settings_unbind (G_OBJECT (self->custom_shortcut_command_entry), "text");

  gtk_widget_hide (self->custom_shortcut_dialog);

  g_object_unref (settings);

  return ret;
}

static gboolean
remove_custom_shortcut (CcKeyboardPanel *self,
                        GtkTreeModel    *model,
                        GtkTreeIter     *iter)
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

  settings_paths = g_settings_get_strv (self->binding_settings, "custom-keybindings");
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));

  for (i = 0; settings_paths[i]; i++)
    if (strcmp (settings_paths[i], item->gsettings_path) != 0)
      g_variant_builder_add (&builder, "s", settings_paths[i]);

  g_settings_set_value (self->binding_settings,
                        "custom-keybindings",
                        g_variant_builder_end (&builder));

  g_strfreev (settings_paths);
  g_object_unref (item);

  keys_array = g_hash_table_lookup (get_hash_for_group (self, BINDING_GROUP_USER), CUSTOM_SHORTCUTS_ID);
  g_ptr_array_remove (keys_array, item);

  gtk_list_store_remove (GTK_LIST_STORE (model), iter);

  return TRUE;
}

static void
add_custom_shortcut (CcKeyboardPanel *self,
                     GtkTreeView     *tree_view,
                     GtkTreeModel    *model)
{
  CcKeyboardItem *item;
  GtkTreePath *path;
  gchar *settings_path;

  item = cc_keyboard_item_new (CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH);

  settings_path = find_free_settings_path (self->binding_settings);
  cc_keyboard_item_load_from_gsettings_path (item, settings_path, TRUE);
  g_free (settings_path);

  item->model = model;

  if (edit_custom_shortcut (self, item) && item->command && item->command[0])
    {
      GPtrArray *keys_array;
      GtkTreeIter iter;
      GHashTable *hash;
      GVariantBuilder builder;
      char **settings_paths;
      int i;

      hash = get_hash_for_group (self, BINDING_GROUP_USER);
      keys_array = g_hash_table_lookup (hash, CUSTOM_SHORTCUTS_ID);
      if (keys_array == NULL)
        {
          keys_array = g_ptr_array_new ();
          g_hash_table_insert (hash, g_strdup (CUSTOM_SHORTCUTS_ID), keys_array);
        }

      g_ptr_array_add (keys_array, item);

      gtk_list_store_append (GTK_LIST_STORE (model), &iter);
      gtk_list_store_set (GTK_LIST_STORE (model), &iter, DETAIL_KEYENTRY_COLUMN, item, -1);

      settings_paths = g_settings_get_strv (self->binding_settings, "custom-keybindings");
      g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
      for (i = 0; settings_paths[i]; i++)
        g_variant_builder_add (&builder, "s", settings_paths[i]);
      g_variant_builder_add (&builder, "s", item->gsettings_path);
      g_settings_set_value (self->binding_settings, "custom-keybindings",
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
update_custom_shortcut (CcKeyboardPanel *self,
                        GtkTreeModel    *model,
                        GtkTreeIter     *iter)
{
  CcKeyboardItem *item;

  gtk_tree_model_get (model, iter,
                      DETAIL_KEYENTRY_COLUMN, &item,
                      -1);

  g_assert (item->type == CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH);

  edit_custom_shortcut (self, item);

  if (item->command == NULL || item->command[0] == '\0')
    {
      remove_custom_shortcut (self, model, iter);
    }
  else
    {
      gtk_list_store_set (GTK_LIST_STORE (model), iter,
                          DETAIL_KEYENTRY_COLUMN, item, -1);
    }
}

static gboolean
start_editing_cb (GtkTreeView    *treeview,
                  GdkEventButton *event,
                  gpointer        user_data)
{
  CcKeyboardPanel *self;
  GtkTreePath *path;
  GtkTreeViewColumn *column;
  GtkCellRenderer *cell = user_data;

  if (event->window != gtk_tree_view_get_bin_window (treeview))
    return FALSE;

  self = CC_KEYBOARD_PANEL (gtk_widget_get_ancestor (GTK_WIDGET (treeview), CC_TYPE_KEYBOARD_PANEL));

  if (gtk_tree_view_get_path_at_pos (treeview,
                                     (gint) event->x,
                                     (gint) event->y,
                                     &path,
                                     &column,
                                     NULL,
                                     NULL))
    {
      GtkTreeModel *model;
      GtkTreeIter iter;
      CcKeyboardItem *item;
      ShortcutType type;

      model = gtk_tree_view_get_model (treeview);
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
          column == gtk_tree_view_get_column (treeview, 0))
        {
          gtk_widget_grab_focus (GTK_WIDGET (treeview));
          gtk_tree_view_set_cursor (treeview,
                                    path,
                                    column,
                                    FALSE);

          update_custom_shortcut (self, model, &iter);
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

      g_signal_stop_emission_by_name (treeview, "button_press_event");
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
  CcKeyboardPanel *self;
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

  self = CC_KEYBOARD_PANEL (gtk_widget_get_ancestor (GTK_WIDGET (treeview), CC_TYPE_KEYBOARD_PANEL));


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
      update_custom_shortcut (self, model, &iter);
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

static gboolean
compare_keys_for_uniqueness (CcKeyboardItem   *element,
                             CcUniquenessData *data)
{
  CcKeyboardItem *orig_item;

  orig_item = data->orig_item;

  /* no conflict for : blanks, different modifiers, or ourselves */
  if (element == NULL ||
      data->new_mask != element->mask ||
      cc_keyboard_item_equal (orig_item, element))
    {
      return FALSE;
    }

  if (data->new_keyval != 0)
    {
      if (data->new_keyval != element->keyval)
          return FALSE;
    }
  else if (element->keyval != 0 || data->new_keycode != element->keycode)
    {
      return FALSE;
    }

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


static CcKeyboardItem *
search_for_conflict_item (CcKeyboardPanel *self,
                          CcKeyboardItem  *item,
                          guint            keyval,
                          GdkModifierType  mask,
                          guint            keycode)
{
  CcUniquenessData data;

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

          table = get_hash_for_group (self, i);
          if (!table)
            continue;
          g_hash_table_find (table, (GHRFunc) cb_check_for_uniqueness, &data);
        }
    }

  return data.conflict_item;
}

static GtkResponseType
show_invalid_binding_dialog (CcKeyboardPanel *self,
                             guint            keyval,
                             GdkModifierType  mask,
                             guint            keycode)
{
  GtkWidget *dialog;
  char *name;

  name = binding_name (keyval, keycode, mask, TRUE);
  dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))),
                                   GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_CANCEL,
                                   _("The shortcut “%s” cannot be used because it will become impossible to type using this key.\n"
                                   "Please try with a key such as Control, Alt or Shift at the same time."),
                                   name);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  g_free (name);

  return GTK_RESPONSE_NONE;
}

static GtkResponseType
show_conflict_item_dialog (CcKeyboardPanel *self,
                           CcKeyboardItem  *item,
                           CcKeyboardItem  *conflict_item,
                           guint            keyval,
                           GdkModifierType  mask,
                           guint            keycode)
{
  GtkWidget *dialog;
  char *name;
  int response;

  name = binding_name (keyval, keycode, mask, TRUE);
  dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))),
                                   GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_CANCEL,
                                   _("The shortcut “%s” is already used for\n“%s”"),
                                   name,
                                   conflict_item->description);

  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            _("If you reassign the shortcut to “%s”, the “%s” shortcut "
                                              "will be disabled."),
                                            item->description,
                                            conflict_item->description);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Reassign"), GTK_RESPONSE_ACCEPT);

  response = gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);

  g_free (name);

  return response;
}


static GtkResponseType
show_reverse_item_dialog (CcKeyboardPanel *self,
                          CcKeyboardItem  *item,
                          CcKeyboardItem  *reverse_item,
                          CcKeyboardItem  *reverse_conflict_item,
                          guint            keyval,
                          GdkModifierType  mask,
                          guint            keycode)
{
  GtkWidget *dialog;
  char *name;
  int response;

  name = binding_name (keyval, keycode, mask, TRUE);

  /* translators:
   * This is the text you get in a dialogue when an action has an associated
   * "reverse" action, for example Alt+Tab going in the opposite direction to
   * Alt+Shift+Tab.
   *
   * An example text would be:
   * The "Switch to next input source" shortcut has an associated "Switch to
   * previous input source" shortcut. Do you want to automatically set it to
   * "Shift+Ctrl+Alt+Space"? */
  dialog = gtk_message_dialog_new (GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (self))),
                                   GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_CANCEL,
                                   _("The “%s” shortcut has an associated “%s” shortcut. "
                                     "Do you want to automatically set it to “%s”?"),
                                   item->description,
                                   reverse_item->description,
                                   name);

  if (reverse_conflict_item != NULL)
    {
      /* translators:
       * This is the text you get in a dialogue when you try to use a shortcut
       * that was already associated with another action, for example:
       * "Alt+F4" is currently associated with "Close Window", ... */
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                                _("“%s” is currently associated with “%s”, this shortcut will be"
                                                  " disabled if you move forward."),
                                                name,
                                                reverse_conflict_item->description);
    }

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Assign"), GTK_RESPONSE_ACCEPT);

  response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  g_free (name);

  return response;
}

static void
handle_reverse_item (CcKeyboardItem  *item,
                     CcKeyboardItem  *reverse_item,
                     guint            keyval,
                     GdkModifierType  mask,
                     guint            keycode,
                     CcKeyboardPanel *self)
{
  GtkResponseType response;
  GdkModifierType reverse_mask;

  reverse_mask = mask ^ GDK_SHIFT_MASK;

  if (!is_valid_binding (keyval, reverse_mask, keycode))
    return;

  if (reverse_item->keyval != keyval ||
      reverse_item->keycode != keycode ||
      reverse_item->mask != reverse_mask)
    {
      CcKeyboardItem *reverse_conflict_item;
      char *binding_str;

      reverse_conflict_item = search_for_conflict_item (self,
                                                        reverse_item,
                                                        keyval,
                                                        reverse_mask,
                                                        keycode);

      response = show_reverse_item_dialog (self,
                                           item,
                                           reverse_item,
                                           reverse_conflict_item,
                                           keyval, reverse_mask,
                                           keycode);
      if (response == GTK_RESPONSE_ACCEPT)
        {
          binding_str = binding_name (keyval, keycode, reverse_mask, FALSE);

          g_object_set (G_OBJECT (reverse_item), "binding", binding_str, NULL);
          g_free (binding_str);

          if (reverse_conflict_item != NULL)
            g_object_set (G_OBJECT (reverse_conflict_item), "binding", NULL, NULL);
        }
      else
        {
          /* The existing reverse binding may be conflicting with the binding
           * we are setting. Other conflicts have already been handled in
           * accel_edited_callback()
           */
          CcKeyboardItem *conflict_item;

          conflict_item = search_for_conflict_item (self, item, keyval, mask, keycode);

          if (conflict_item != NULL)
            {
              g_warn_if_fail (conflict_item == reverse_item);
              g_object_set (G_OBJECT (conflict_item), "binding", NULL, NULL);
            }
        }
    }
}

static void
accel_edited_callback (GtkCellRendererText   *cell,
                       const char            *path_string,
                       guint                  keyval,
                       GdkModifierType        mask,
                       guint                  keycode,
                       CcKeyboardPanel       *self)
{
  GtkTreeModel *model;
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  GtkTreeIter iter;
  CcKeyboardItem *item;
  CcKeyboardItem *conflict_item;
  CcKeyboardItem *reverse_item;
  char *str;

  model = gtk_tree_view_get_model (GTK_TREE_VIEW (self->shortcut_treeview));
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

  conflict_item = search_for_conflict_item (self, item, keyval, mask, keycode);

  /* Check for unmodified keys */
  if (!is_valid_binding (keyval, mask, keycode))
    {
      show_invalid_binding_dialog (self, keyval, mask, keycode);

      /* set it back to its previous value. */
      g_object_set (G_OBJECT (cell),
                    "accel-key", item->keyval,
                    "keycode", item->keycode,
                    "accel-mods", item->mask,
                    NULL);
      return;
    }

  reverse_item = cc_keyboard_item_get_reverse_item (item);

  /* flag to see if the new accelerator was in use by something */
  if ((conflict_item != NULL) && (conflict_item != reverse_item))
    {
      GtkResponseType response;

      response = show_conflict_item_dialog (self,
                                            item,
                                            conflict_item,
                                            keyval,
                                            mask,
                                            keycode);

      if (response == GTK_RESPONSE_ACCEPT)
        {
          g_object_set (G_OBJECT (conflict_item), "binding", NULL, NULL);

          str = binding_name (keyval, keycode, mask, FALSE);
          g_object_set (G_OBJECT (item), "binding", str, NULL);

          g_free (str);
          if (reverse_item == NULL)
            return;
        }
      else
        {
          /* set it back to its previous value. */
          g_object_set (G_OBJECT (cell),
                        "accel-key", item->keyval,
                        "keycode", item->keycode,
                        "accel-mods", item->mask,
                        NULL);
          return;
        }

    }

  str = binding_name (keyval, keycode, mask, FALSE);
  g_object_set (G_OBJECT (item), "binding", str, NULL);

  if (reverse_item != NULL)
    handle_reverse_item (item, reverse_item, keyval, mask, keycode, self);

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
  g_object_set (G_OBJECT (item), "binding", NULL, NULL);
}

static void
shortcut_entry_changed (GtkEntry        *entry,
                        CcKeyboardPanel *self)
{
  guint16 name_length;
  guint16 command_length;

  name_length = gtk_entry_get_text_length (GTK_ENTRY (self->custom_shortcut_name_entry));
  command_length = gtk_entry_get_text_length (GTK_ENTRY (self->custom_shortcut_command_entry));

  gtk_widget_set_sensitive (self->custom_shortcut_ok_button, name_length > 0 && command_length > 0);
}

static void
add_button_clicked (GtkWidget       *button,
                    CcKeyboardPanel *self)
{
  GtkTreeView *treeview;
  GtkTreeModel *model;

  treeview = GTK_TREE_VIEW (self->shortcut_treeview);
  model = gtk_tree_view_get_model (treeview);

  /* And add the shortcut */
  add_custom_shortcut (self, treeview, model);
}

static void
remove_button_clicked (GtkWidget       *button,
                       CcKeyboardPanel *self)
{
  GtkTreeView *treeview;
  GtkTreeModel *model;
  GtkTreeSelection *selection;
  GtkTreeIter iter;

  treeview = GTK_TREE_VIEW (self->shortcut_treeview);
  model = gtk_tree_view_get_model (treeview);
  selection = gtk_tree_view_get_selection (treeview);

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    {
      remove_custom_shortcut (self, model, &iter);
    }
}

static void
xkb_options_combo_changed (GtkCellRendererCombo *combo,
                           gchar                *model_path,
                           GtkTreeIter          *model_iter,
                           CcKeyboardPanel      *self)
{
  GtkTreeView *shortcut_treeview;
  GtkTreeModel *shortcut_model;
  GtkTreeIter shortcut_iter;
  GtkTreeSelection *selection;
  CcKeyboardOption *option;
  ShortcutType type;

  shortcut_treeview = GTK_TREE_VIEW (self->shortcut_treeview);
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

static void
setup_tree_views (CcKeyboardPanel *self)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer *renderer;
  GtkListStore *model;
  GtkWidget *widget;
  CcShell *shell;
  GList *focus_chain;

  /* Setup the section treeview */
  self->sections_store = gtk_list_store_new (SECTION_N_COLUMNS,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_INT);
  self->sections_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (self->sections_store));

  gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (self->sections_model),
                                   SECTION_DESCRIPTION_COLUMN,
                                   section_sort_item,
                                   self,
                                   NULL);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (self->sections_model),
                                        SECTION_DESCRIPTION_COLUMN,
                                        GTK_SORT_ASCENDING);

  /* Setup the shortcut treeview */
  renderer = gtk_cell_renderer_text_new ();
  g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);

  column = gtk_tree_view_column_new_with_attributes (NULL, renderer, NULL);
  gtk_tree_view_column_set_cell_data_func (column, renderer, description_set_func, NULL, NULL);
  gtk_tree_view_column_set_resizable (column, FALSE);
  gtk_tree_view_column_set_expand (column, TRUE);

  gtk_tree_view_append_column (GTK_TREE_VIEW (self->shortcut_treeview), column);

  renderer = (GtkCellRenderer *) g_object_new (GTK_TYPE_CELL_RENDERER_ACCEL,
                                               "accel-mode", GTK_CELL_RENDERER_ACCEL_MODE_OTHER,
                                               NULL);

  g_signal_connect (self->shortcut_treeview,
                    "button_press_event",
                    G_CALLBACK (start_editing_cb),
                    renderer);
  g_signal_connect (self->shortcut_treeview,
                    "row-activated",
                    G_CALLBACK (start_editing_kb_cb),
                    renderer);

  g_signal_connect (renderer,
                    "accel_edited",
                    G_CALLBACK (accel_edited_callback),
                    self);
  g_signal_connect (renderer,
                    "accel_cleared",
                    G_CALLBACK (accel_cleared_callback),
                    self->shortcut_treeview);

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
  g_signal_connect (renderer,
                    "changed",
                    G_CALLBACK (xkb_options_combo_changed),
                    self);

  gtk_tree_view_column_pack_end (column, renderer, FALSE);

  gtk_tree_view_column_set_cell_data_func (column, renderer, accel_set_func, NULL, NULL);

  gtk_tree_view_append_column (GTK_TREE_VIEW (self->shortcut_treeview), column);

  model = gtk_list_store_new (DETAIL_N_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_INT);
  gtk_tree_view_set_model (GTK_TREE_VIEW (self->shortcut_treeview), GTK_TREE_MODEL (model));
  g_object_unref (model);

  setup_keyboard_options (model);

  /* set up the focus chain */
  focus_chain = g_list_append (NULL, self->shortcut_treeview);
  focus_chain = g_list_append (focus_chain, self->shortcut_toolbar);

  gtk_container_set_focus_chain (GTK_CONTAINER (self), focus_chain);
  g_list_free (focus_chain);

  /* set up the dialog */
  shell = cc_panel_get_shell (CC_PANEL (self));
  widget = cc_shell_get_toplevel (shell);

  gtk_dialog_set_default_response (GTK_DIALOG (self->custom_shortcut_dialog), GTK_RESPONSE_OK);
  gtk_window_set_transient_for (GTK_WINDOW (self->custom_shortcut_dialog), GTK_WINDOW (widget));
}

static void
cc_keyboard_panel_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  switch (property_id)
    {
    case PROP_PARAMETERS:
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static const char *
cc_keyboard_panel_get_help_uri (CcPanel *panel)
{
  return "help:gnome-help/keyboard";
}

static void
cc_keyboard_panel_finalize (GObject *object)
{
  CcKeyboardPanel *self = CC_KEYBOARD_PANEL (object);

  g_clear_pointer (&self->kb_system_sections, g_hash_table_destroy);
  g_clear_pointer (&self->kb_apps_sections, g_hash_table_destroy);
  g_clear_pointer (&self->kb_user_sections, g_hash_table_destroy);
  g_clear_pointer (&self->pictures_regex, g_regex_unref);
  g_clear_pointer (&self->wm_changed_id, wm_common_unregister_window_manager_change);

  g_clear_object (&self->custom_shortcut_dialog);
  g_clear_object (&self->binding_settings);
  g_clear_object (&self->sections_store);
  g_clear_object (&self->sections_model);

  cc_keyboard_option_clear_all ();

  G_OBJECT_CLASS (cc_keyboard_panel_parent_class)->finalize (object);
}

static void
on_window_manager_change (const char      *wm_name,
                          CcKeyboardPanel *self)
{
  reload_sections (self);
}

static void
cc_keyboard_panel_constructed (GObject *object)
{
  CcKeyboardPanel *self = CC_KEYBOARD_PANEL (object);

  G_OBJECT_CLASS (cc_keyboard_panel_parent_class)->constructed (object);

#ifdef GDK_WINDOWING_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    self->wm_changed_id = wm_common_register_window_manager_change ((GFunc) on_window_manager_change,
                                                                    self);
#endif

  setup_tree_views (self);
  reload_sections (self);

  add_shortcuts (self);
}

static void
cc_keyboard_panel_class_init (CcKeyboardPanelClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

  panel_class->get_help_uri = cc_keyboard_panel_get_help_uri;

  object_class->set_property = cc_keyboard_panel_set_property;
  object_class->finalize = cc_keyboard_panel_finalize;
  object_class->constructed = cc_keyboard_panel_constructed;

  g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/keyboard/gnome-keyboard-panel.ui");

  gtk_widget_class_bind_template_child (widget_class, CcKeyboardPanel, add_toolbutton);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardPanel, custom_shortcut_command_entry);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardPanel, custom_shortcut_dialog);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardPanel, custom_shortcut_name_entry);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardPanel, custom_shortcut_ok_button);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardPanel, remove_toolbutton);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardPanel, shortcut_toolbar);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardPanel, shortcut_treeview);

  gtk_widget_class_bind_template_callback (widget_class, add_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, remove_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, shortcut_entry_changed);
  gtk_widget_class_bind_template_callback (widget_class, shortcut_selection_changed);
}

static void
cc_keyboard_panel_init (CcKeyboardPanel *self)
{
  g_resources_register (cc_keyboard_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->binding_settings = g_settings_new (BINDINGS_SCHEMA);
}
