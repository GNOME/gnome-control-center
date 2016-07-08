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
#include "cc-keyboard-shortcut-editor.h"

#include "keyboard-shortcuts.h"
#include "wm-common.h"

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#define BINDINGS_SCHEMA       "org.gnome.settings-daemon.plugins.media-keys"
#define CUSTOM_SHORTCUTS_ID   "custom"

typedef struct {
  CcKeyboardItem *item;
  gchar          *section_title;
  gchar          *section_id;
} RowData;

struct _CcKeyboardPanel
{
  CcPanel             parent;

  /* Shortcut models */
  GtkListStore       *shortcuts_model;
  GtkListStore       *sections_store;
  GtkTreeModel       *sections_model;
  GtkWidget          *listbox;
  GtkListBoxRow      *add_shortcut_row;
  GtkSizeGroup       *accelerator_sizegroup;

  /* Custom shortcut dialog */
  GtkWidget          *shortcut_editor;

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

/* RowData functions */
static RowData *
row_data_new (CcKeyboardItem *item,
              const gchar    *section_id,
              const gchar    *section_title)
{
  RowData *data;

  data = g_new0 (RowData, 1);
  data->item = g_object_ref (item);
  data->section_id = g_strdup (section_id);
  data->section_title = g_strdup (section_title);

  return data;
}

static void
row_data_free (RowData *data)
{
  g_object_unref (data->item);
  g_free (data->section_id);
  g_free (data->section_title);
  g_free (data);
}

static gboolean
transform_binding_to_accel (GBinding     *binding,
                            const GValue *from_value,
                            GValue       *to_value,
                            gpointer      user_data)
{
  CcKeyboardItem *item;
  gchar *accelerator;

  item = CC_KEYBOARD_ITEM (g_binding_get_source (binding));

  accelerator = convert_keysym_state_to_string (item->keyval, item->mask, item->keycode);

  g_value_take_string (to_value, accelerator);

  return TRUE;
}

static void
add_item (CcKeyboardPanel *self,
          CcKeyboardItem  *item,
          const gchar     *section_id,
          const gchar     *section_title)
{
  GtkWidget *row, *box, *label;

  /* Horizontal box */
  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_set_border_width (GTK_CONTAINER (box), 6);

  /* Shortcut title */
  label = gtk_label_new (item->description);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
  gtk_label_set_line_wrap_mode (GTK_LABEL (label), PANGO_WRAP_WORD_CHAR);
  gtk_widget_set_hexpand (label, TRUE);

  g_object_bind_property (item,
                          "description",
                          label,
                          "label",
                          G_BINDING_DEFAULT);

  gtk_container_add (GTK_CONTAINER (box), label);

  /* Shortcut accelerator */
  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);

  gtk_size_group_add_widget (self->accelerator_sizegroup, label);

  g_object_bind_property_full (item,
                               "binding",
                               label,
                              "label",
                               G_BINDING_DEFAULT | G_BINDING_SYNC_CREATE,
                               transform_binding_to_accel,
                               NULL, NULL, NULL);

  gtk_container_add (GTK_CONTAINER (box), label);

  gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");

  /* The row */
  row = gtk_list_box_row_new ();
  gtk_container_add (GTK_CONTAINER (row), box);

  gtk_widget_show_all (row);

  g_object_set_data_full (G_OBJECT (row),
                          "data",
                          row_data_new (item, section_id, section_title),
                          (GDestroyNotify) row_data_free);

  gtk_container_add (GTK_CONTAINER (self->listbox), row);
}

static void
remove_item (CcKeyboardPanel *self,
             CcKeyboardItem  *item)
{
  GList *children, *l;

  children = gtk_container_get_children (GTK_CONTAINER (self->listbox));

  for (l = children; l != NULL; l = l->next)
    {
      RowData *row_data;

      row_data = g_object_get_data (l->data, "data");

      if (row_data->item == item)
        {
          gtk_container_remove (GTK_CONTAINER (self->listbox), l->data);
          break;
        }
    }

  g_list_free (children);
}

static gint
sort_function (GtkListBoxRow *a,
               GtkListBoxRow *b,
               gpointer       user_data)
{
  CcKeyboardPanel *self;
  RowData *a_data, *b_data;
  gint retval;

  self = user_data;

  if (a == self->add_shortcut_row)
    return 1;

  if (b == self->add_shortcut_row)
    return -1;

  a_data = g_object_get_data (G_OBJECT (a), "data");
  b_data = g_object_get_data (G_OBJECT (b), "data");

  /* Put custom shortcuts below everything else */
  if (a_data->item->type == CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH)
    return 1;
  else if (b_data->item->type == CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH)
    return -1;

  retval = g_strcmp0 (a_data->section_title, b_data->section_title);

  if (retval != 0)
    return retval;

  return g_strcmp0 (a_data->item->description, b_data->item->description);
}

static void
header_function (GtkListBoxRow *row,
                 GtkListBoxRow *before,
                 gpointer       user_data)
{
  CcKeyboardPanel *self;
  gboolean add_header;
  RowData *data;

  self = user_data;
  add_header = FALSE;

  /* The + row always has a separator */
  if (row == self->add_shortcut_row)
    {
      GtkWidget *separator = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_widget_show (separator);

      gtk_list_box_row_set_header (row, separator);

      return;
    }

  data = g_object_get_data (G_OBJECT (row), "data");

  if (before)
    {
      RowData *before_data = g_object_get_data (G_OBJECT (before), "data");

      if (before_data)
        add_header = g_strcmp0 (before_data->section_id, data->section_id) != 0;
    }
  else
    {
      add_header = TRUE;
    }

  if (add_header)
    {
      GtkWidget *box, *label;
      gchar *markup;

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
      gtk_widget_set_margin_top (box, before ? 18 : 6);

      markup = g_strdup_printf ("<b>%s</b>", _(data->section_title));
      label = g_object_new (GTK_TYPE_LABEL,
                            "label", markup,
                            "use-markup", TRUE,
                            "xalign", 0.0,
                            "margin-start", 6,
                            NULL);

      gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");

      gtk_container_add (GTK_CONTAINER (box), label);
      gtk_container_add (GTK_CONTAINER (box), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL));

      gtk_list_box_row_set_header (row, box);

      gtk_widget_show_all (box);

      g_free (markup);
    }
  else
    {
      gtk_list_box_row_set_header (row, NULL);
    }
}

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

  shortcut_model = GTK_TREE_MODEL (self->shortcuts_model);

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
      item->group = group;

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
  GHashTable *loaded_files;
  GDir *dir;
  gchar *default_wm_keybindings[] = { "Mutter", "GNOME Shell", NULL };
  gchar **wm_keybindings;
  const gchar * const * data_dirs;
  guint i;

  shortcut_model = GTK_TREE_MODEL (self->shortcuts_model);
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
  GtkTreeIter sections_iter;
  gboolean can_continue;

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

              gtk_list_store_append (self->shortcuts_model, &new_row);
              gtk_list_store_set (self->shortcuts_model,
                                  &new_row,
                                  DETAIL_DESCRIPTION_COLUMN, item->description,
                                  DETAIL_KEYENTRY_COLUMN, item,
                                  DETAIL_TYPE_COLUMN, SHORTCUT_TYPE_KEY_ENTRY,
                                  -1);

              add_item (self, item, id, title);
            }
        }

      can_continue = gtk_tree_model_iter_next (self->sections_model, &sections_iter);

      g_free (title);
      g_free (id);
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
remove_custom_shortcut (CcKeyboardShortcutEditor *editor,
                        CcKeyboardItem           *item,
                        CcKeyboardPanel          *self)
{
  GtkTreeModel *model;
  GtkTreeIter iter;
  GPtrArray *keys_array;
  GVariantBuilder builder;
  gboolean valid;
  char **settings_paths;
  int i;

  model = GTK_TREE_MODEL (self->shortcuts_model);
  valid = gtk_tree_model_get_iter_first (model, &iter);

  /* Search for the iter */
  while (valid)
    {
      CcKeyboardItem  *current_item;

      gtk_tree_model_get (model, &iter,
                          DETAIL_KEYENTRY_COLUMN, &current_item,
                          -1);

      if (current_item == item)
        break;

      valid = gtk_tree_model_iter_next (model, &iter);

      g_clear_object (&current_item);
    }

  if (!valid)
    g_error ("Tried to remove a non-existant shortcut");

  g_assert (item->type == CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH);

  remove_item (self, item);

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

  gtk_list_store_remove (GTK_LIST_STORE (model), &iter);

  return TRUE;
}

static void
add_custom_shortcut (CcKeyboardShortcutEditor *editor,
                     CcKeyboardItem           *item,
                     CcKeyboardPanel          *self)
{
  GtkTreePath *path;
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

  gtk_list_store_append (self->shortcuts_model, &iter);
  gtk_list_store_set (self->shortcuts_model, &iter, DETAIL_KEYENTRY_COLUMN, item, -1);

  settings_paths = g_settings_get_strv (self->binding_settings, "custom-keybindings");
  g_variant_builder_init (&builder, G_VARIANT_TYPE ("as"));
  for (i = 0; settings_paths[i]; i++)
    g_variant_builder_add (&builder, "s", settings_paths[i]);
  g_variant_builder_add (&builder, "s", item->gsettings_path);
  g_settings_set_value (self->binding_settings, "custom-keybindings",
                        g_variant_builder_end (&builder));

  /* make the new shortcut visible */
  path = gtk_tree_model_get_path (GTK_TREE_MODEL (self->shortcuts_model), &iter);
  gtk_tree_path_free (path);

  add_item (self, item, CUSTOM_SHORTCUTS_ID, _("Custom Shortcuts"));
}

static void
shortcut_row_activated (GtkWidget       *button,
                        GtkListBoxRow   *row,
                        CcKeyboardPanel *self)
{
  CcKeyboardShortcutEditor *editor;

  editor = CC_KEYBOARD_SHORTCUT_EDITOR (self->shortcut_editor);

  if (row != self->add_shortcut_row)
    {
      RowData *data = g_object_get_data (G_OBJECT (row), "data");

      cc_keyboard_shortcut_editor_set_mode (editor, CC_SHORTCUT_EDITOR_EDIT);
      cc_keyboard_shortcut_editor_set_item (editor, data->item);
    }
  else
    {
      cc_keyboard_shortcut_editor_set_mode (editor, CC_SHORTCUT_EDITOR_CREATE);
      cc_keyboard_shortcut_editor_set_item (editor, NULL);
    }

  gtk_widget_show (self->shortcut_editor);
}

static void
setup_tree_views (CcKeyboardPanel *self)
{
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

  self->shortcuts_model = gtk_list_store_new (DETAIL_N_COLUMNS,
                                              G_TYPE_STRING,
                                              G_TYPE_POINTER,
                                              G_TYPE_INT);

  setup_keyboard_options (self->shortcuts_model);
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

  g_clear_object (&self->accelerator_sizegroup);
  g_clear_object (&self->binding_settings);
  g_clear_object (&self->shortcuts_model);
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
  GtkWindow *toplevel;

  G_OBJECT_CLASS (cc_keyboard_panel_parent_class)->constructed (object);

  /* Setup the dialog's transient parent */
  toplevel = GTK_WINDOW (cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (self))));
  gtk_window_set_transient_for (GTK_WINDOW (self->shortcut_editor), toplevel);

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

  gtk_widget_class_bind_template_child (widget_class, CcKeyboardPanel, add_shortcut_row);
  gtk_widget_class_bind_template_child (widget_class, CcKeyboardPanel, listbox);

  gtk_widget_class_bind_template_callback (widget_class, shortcut_row_activated);
  gtk_widget_class_bind_template_callback (widget_class, shortcut_selection_changed);
}

static void
cc_keyboard_panel_init (CcKeyboardPanel *self)
{
  g_resources_register (cc_keyboard_get_resource ());

  gtk_widget_init_template (GTK_WIDGET (self));

  self->binding_settings = g_settings_new (BINDINGS_SCHEMA);

  /* Use a sizegroup to make the accelerator labels the same width */
  self->accelerator_sizegroup = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  /* Shortcut editor dialog */
  self->shortcut_editor = cc_keyboard_shortcut_editor_new (self);

  g_signal_connect (self->shortcut_editor,
                    "add-custom-shortcut",
                    G_CALLBACK (add_custom_shortcut),
                    self);

  g_signal_connect (self->shortcut_editor,
                    "remove-custom-shortcut",
                    G_CALLBACK (remove_custom_shortcut),
                    self);

  /* Setup the shortcuts listbox */
  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->listbox),
                              sort_function,
                              self,
                              NULL);

  gtk_list_box_set_header_func (GTK_LIST_BOX (self->listbox),
                                header_function,
                                self,
                                NULL);
}

/**
 * cc_keyboard_panel_create_custom_item:
 * @self: a #CcKeyboardPanel
 *
 * Creates a new temporary keyboard shortcut.
 *
 * Returns: (transfer full): a #CcKeyboardItem
 */
CcKeyboardItem*
cc_keyboard_panel_create_custom_item (CcKeyboardPanel *self)
{
  CcKeyboardItem *item;
  gchar *settings_path;

  g_return_val_if_fail (CC_IS_KEYBOARD_PANEL (self), NULL);

  item = cc_keyboard_item_new (CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH);

  settings_path = find_free_settings_path (self->binding_settings);
  cc_keyboard_item_load_from_gsettings_path (item, settings_path, TRUE);
  g_free (settings_path);

  item->model = GTK_TREE_MODEL (self->shortcuts_model);
  item->group = BINDING_GROUP_USER;

  return item;
}

