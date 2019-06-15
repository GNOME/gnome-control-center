/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011, 2014 Red Hat, Inc.
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
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>

#include "cc-keyboard-item.h"

#define CUSTOM_KEYS_SCHEMA "org.gnome.settings-daemon.plugins.media-keys.custom-keybinding"

struct _CcKeyboardItem
{
  GObject parent_instance;

  char *binding;

  CcKeyboardItem *reverse_item;
  gboolean is_reversed;
  gboolean hidden;

  CcKeyboardItemType type;

  CcKeyCombo *primary_combo;
  BindingGroupType group;
  GtkTreeModel *model;
  char *description;
  gboolean editable;
  GList *key_combos;
  GList *default_combos;

  /* GSettings path */
  char *gsettings_path;
  gboolean desc_editable;
  char *command;
  gboolean cmd_editable;

  /* GSettings */
  char *schema;
  char *key;
  GSettings *settings;
};

enum
{
  PROP_0,
  PROP_DESCRIPTION,
  PROP_BINDING,
  PROP_EDITABLE,
  PROP_TYPE,
  PROP_IS_VALUE_DEFAULT,
  PROP_COMMAND
};

static void     cc_keyboard_item_class_init     (CcKeyboardItemClass *klass);
static void     cc_keyboard_item_init           (CcKeyboardItem      *keyboard_item);
static void     cc_keyboard_item_finalize       (GObject               *object);

G_DEFINE_TYPE (CcKeyboardItem, cc_keyboard_item, G_TYPE_OBJECT)

static const gchar *
get_binding_from_variant (GVariant *variant)
{
  const char *str, **strv;

  if (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING))
    return g_variant_get_string (variant, NULL);
  else if (!g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING_ARRAY))
    return "";

  strv = g_variant_get_strv (variant, NULL);
  str = strv[0];
  g_free (strv);

  return str;
}

static gboolean
binding_from_string (const char *str,
                     CcKeyCombo *combo)
{
  g_return_val_if_fail (combo != NULL, FALSE);
  guint *keycodes;

  if (str == NULL || strcmp (str, "disabled") == 0)
    {
      memset (combo, 0, sizeof(CcKeyCombo));
      return TRUE;
    }

  gtk_accelerator_parse_with_keycode (str, &combo->keyval, &keycodes, &combo->mask);

  combo->keycode = (keycodes ? keycodes[0] : 0);
  g_free (keycodes);

  if (combo->keyval == 0)
    return FALSE;
  else
    return TRUE;
}

static void
_set_description (CcKeyboardItem *item,
                  const char       *value)
{
  g_free (item->description);
  item->description = g_strdup (value);
}

const char *
cc_keyboard_item_get_description (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), NULL);

  return item->description;
}

gboolean
cc_keyboard_item_get_desc_editable (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), FALSE);

  return item->desc_editable;
}

/* wrapper around g_settings_set_str[ing|v] */
static void
settings_set_binding (GSettings  *settings,
                      const char *key,
		      const char *value)
{
  GVariant *variant;

  variant = g_settings_get_value (settings, key);

  if (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING))
    g_settings_set_string (settings, key, value ? value : "");
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING_ARRAY))
    {
      if (value == NULL || *value == '\0')
        g_settings_set_strv (settings, key, NULL);
      else
        {
          char **str_array = g_new0 (char *, 2);

          /* clear any additional bindings by only setting the first one */
          *str_array = g_strdup (value);

          g_settings_set_strv (settings, key, (const char * const *)str_array);
          g_strfreev (str_array);
        }
    }

  g_variant_unref (variant);
}


static void
_set_binding (CcKeyboardItem *item,
              const char     *value,
	      gboolean        set_backend)
{
  CcKeyboardItem *reverse;
  gboolean enabled;

  reverse = item->reverse_item;
  enabled = value && strlen (value) > 0;

  g_clear_pointer (&item->binding, g_free);
  item->binding = enabled ? g_strdup (value) : g_strdup ("");

  binding_from_string (item->binding, item->primary_combo);

  /*
   * Always treat the pair (item, reverse) as a unit: setting one also
   * disables the other, disabling one up also sets the other.
   */
  if (reverse)
    {
      GdkModifierType reverse_mask;

      reverse_mask = enabled ? item->primary_combo->mask ^ GDK_SHIFT_MASK
                             : item->primary_combo->mask;

      g_clear_pointer (&reverse->binding, g_free);
      if (enabled)
        reverse->binding = gtk_accelerator_name_with_keycode (NULL,
                                                                    item->primary_combo->keyval,
                                                                    item->primary_combo->keycode,
                                                                    reverse_mask);

      binding_from_string (reverse->binding, reverse->primary_combo);
    }

  if (set_backend == FALSE)
    return;

  settings_set_binding (item->settings, item->key, item->binding);

  g_object_notify (G_OBJECT (item), "is-value-default");

  if (reverse)
    {
      settings_set_binding (reverse->settings, reverse->key, reverse->binding);
      g_object_notify (G_OBJECT (reverse), "is-value-default");
    }
}

static void
_set_type (CcKeyboardItem *item,
           gint            value)
{
  item->type = value;
}

static void
_set_command (CcKeyboardItem *item,
              const char       *value)
{
  g_free (item->command);
  item->command = g_strdup (value);
}

const char *
cc_keyboard_item_get_command (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), NULL);

  return item->command;
}

gboolean
cc_keyboard_item_get_cmd_editable (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), FALSE);

  return item->cmd_editable;
}

static void
cc_keyboard_item_set_property (GObject      *object,
                               guint         prop_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
  CcKeyboardItem *self;

  self = CC_KEYBOARD_ITEM (object);

  switch (prop_id) {
  case PROP_DESCRIPTION:
    _set_description (self, g_value_get_string (value));
    break;
  case PROP_BINDING:
    _set_binding (self, g_value_get_string (value), TRUE);
    break;
  case PROP_COMMAND:
    _set_command (self, g_value_get_string (value));
    break;
  case PROP_TYPE:
    _set_type (self, g_value_get_int (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
cc_keyboard_item_get_property (GObject    *object,
                               guint       prop_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
  CcKeyboardItem *self;

  self = CC_KEYBOARD_ITEM (object);

  switch (prop_id) {
  case PROP_DESCRIPTION:
    g_value_set_string (value, self->description);
    break;
  case PROP_BINDING:
    g_value_set_string (value, self->binding);
    break;
  case PROP_EDITABLE:
    g_value_set_boolean (value, self->editable);
    break;
  case PROP_COMMAND:
    g_value_set_string (value, self->command);
    break;
  case PROP_IS_VALUE_DEFAULT:
    g_value_set_boolean (value, cc_keyboard_item_is_value_default (self));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static void
cc_keyboard_item_class_init (CcKeyboardItemClass *klass)
{
  GObjectClass  *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = cc_keyboard_item_get_property;
  object_class->set_property = cc_keyboard_item_set_property;
  object_class->finalize = cc_keyboard_item_finalize;

  g_object_class_install_property (object_class,
                                   PROP_DESCRIPTION,
                                   g_param_spec_string ("description",
                                                        "description",
                                                        "description",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_BINDING,
                                   g_param_spec_string ("binding",
                                                        "binding",
                                                        "binding",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_EDITABLE,
                                   g_param_spec_boolean ("editable",
                                                         NULL,
                                                         NULL,
                                                         FALSE,
                                                         G_PARAM_READABLE));

  g_object_class_install_property (object_class,
                                   PROP_TYPE,
                                   g_param_spec_int ("type",
                                                     NULL,
                                                     NULL,
                                                     CC_KEYBOARD_ITEM_TYPE_NONE,
                                                     CC_KEYBOARD_ITEM_TYPE_GSETTINGS,
                                                     CC_KEYBOARD_ITEM_TYPE_NONE,
                                                     G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
                                   PROP_COMMAND,
                                   g_param_spec_string ("command",
                                                        "command",
                                                        "command",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_IS_VALUE_DEFAULT,
                                   g_param_spec_boolean ("is-value-default",
                                                         "is value default",
                                                         "is value default",
                                                         TRUE,
                                                         G_PARAM_READABLE));
}

static void
cc_keyboard_item_init (CcKeyboardItem *item)
{
  item->primary_combo = g_new0 (CcKeyCombo, 1);
}

static void
cc_keyboard_item_finalize (GObject *object)
{
  CcKeyboardItem *item;

  g_return_if_fail (object != NULL);
  g_return_if_fail (CC_IS_KEYBOARD_ITEM (object));

  item = CC_KEYBOARD_ITEM (object);

  if (item->settings != NULL)
    g_object_unref (item->settings);

  /* Free memory */
  g_free (item->binding);
  g_free (item->primary_combo);
  g_free (item->gsettings_path);
  g_free (item->description);
  g_free (item->command);
  g_free (item->schema);
  g_free (item->key);
  g_list_free_full (item->key_combos, g_free);
  g_list_free_full (item->default_combos, g_free);

  G_OBJECT_CLASS (cc_keyboard_item_parent_class)->finalize (object);
}

CcKeyboardItem *
cc_keyboard_item_new (CcKeyboardItemType type)
{
  GObject *object;

  object = g_object_new (CC_TYPE_KEYBOARD_ITEM,
                         "type", type,
                         NULL);

  return CC_KEYBOARD_ITEM (object);
}

static guint *
get_above_tab_keysyms (void)
{
  GdkKeymap *keymap = gdk_keymap_get_for_display (gdk_display_get_default ());
  guint keycode = 0x29 /* KEY_GRAVE */ + 8;
  g_autofree guint *keyvals = NULL;
  GArray *keysyms;
  int n_entries, i, j;

  keysyms = g_array_new (TRUE, FALSE, sizeof (guint));

  if (!gdk_keymap_get_entries_for_keycode (keymap, keycode, NULL, &keyvals, &n_entries))
    goto out;

  for (i = 0; i < n_entries; i++)
    {
      gboolean found = FALSE;

      for (j = 0; j < keysyms->len; j++)
        if (g_array_index (keysyms, guint, j) == keyvals[i])
          {
            found = TRUE;
            break;
          }

      if (!found)
        g_array_append_val (keysyms, keyvals[i]);
    }

out:
  return (guint *)g_array_free (keysyms, FALSE);
}

/*
 * translate_above_tab:
 *
 * @original_bindings: A list of accelerator strings
 * @new_bindings: (out): Translated bindings if translation is needed
 *
 * Translate accelerator strings that contain the Above_Tab fake keysym
 * used by mutter to strings that use the real keysyms that correspond
 * to the key that is located physically above the tab key.
 *
 * Returns: %TRUE if strings were translated, %FALSE if @original_bindings
 *   can be used unmodified
 */
static gboolean
translate_above_tab (char **original_bindings,
                     char ***new_bindings)
{
  GPtrArray *replaced_bindings;
  g_autofree guint *above_tab_keysyms = NULL;
  gboolean needs_translation = FALSE;
  char **str;

  for (str = original_bindings; *str && !needs_translation; str++)
    needs_translation = strstr (*str, "Above_Tab") != NULL;

  if (!needs_translation)
    return FALSE;

  above_tab_keysyms = get_above_tab_keysyms ();

  replaced_bindings = g_ptr_array_new ();

  for (str = original_bindings; *str; str++)
    {
      if (strstr (*str, "Above_Tab") == NULL)
        {
          g_ptr_array_add (replaced_bindings, g_strdup (*str));
        }
      else
        {
          g_auto (GStrv) split_str = g_strsplit (*str, "Above_Tab", -1);
          int i;

          for (i = 0; above_tab_keysyms[i]; i++)
            {
              g_autofree char *sym = NULL;

              sym = gtk_accelerator_name (above_tab_keysyms[i], 0);
              g_ptr_array_add (replaced_bindings, g_strjoinv (sym, split_str));
            }
        }
      g_ptr_array_add (replaced_bindings, NULL);
    }

  *new_bindings = (char **)g_ptr_array_free (replaced_bindings, FALSE);
  return TRUE;
}

static char *
translate_binding_string (const char *str)
{
  g_autofree guint *above_tab_keysyms = NULL;
  g_autofree char *symname = NULL;
  g_auto (GStrv) split_str = NULL;

  if (str == NULL || strstr (str, "Above_Tab") == NULL)
    return g_strdup (str);

  above_tab_keysyms = get_above_tab_keysyms ();
  symname = gtk_accelerator_name (above_tab_keysyms[0], 0);

  split_str = g_strsplit (str, "Above_Tab", -1);
  return g_strjoinv (symname, split_str);
}

static GList *
variant_get_key_combos (GVariant *variant)
{
  GList *combos = NULL;
  char **bindings = NULL, **translated_bindings, **str;

  if (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING))
    {
      bindings = g_malloc0_n (2, sizeof(char *));
      bindings[0] = g_variant_dup_string (variant, NULL);
    }
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING_ARRAY))
    {
      bindings = g_variant_dup_strv (variant, NULL);
    }

  if (translate_above_tab (bindings, &translated_bindings))
    {
      g_strfreev (bindings);
      bindings = translated_bindings;
    }

  for (str = bindings; *str; str++)
    {
      CcKeyCombo *combo = g_new (CcKeyCombo, 1);

      binding_from_string (*str, combo);
      combos = g_list_prepend (combos, combo);
    }
  g_strfreev (bindings);

  return g_list_reverse (combos);
}

static GList *
settings_get_key_combos (GSettings  *settings,
                         const char *key,
                         gboolean    use_default)
{
  GList *key_combos;
  GVariant *variant;

  if (use_default)
    variant = g_settings_get_default_value (settings, key);
  else
    variant = g_settings_get_value (settings, key);
  key_combos = variant_get_key_combos (variant);
  g_variant_unref (variant);

  return key_combos;
}

/* wrapper around g_settings_get_str[ing|v] */
static char *
settings_get_binding (GSettings  *settings,
                      const char *key)
{
  GVariant *variant;
  char *value = NULL;

  variant = g_settings_get_value (settings, key);
  if (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING))
    value = translate_binding_string (g_variant_get_string (variant, NULL));
  else if (g_variant_is_of_type (variant, G_VARIANT_TYPE_STRING_ARRAY))
    {
      const char **str_array;

      str_array = g_variant_get_strv (variant, NULL);
      value = translate_binding_string (str_array[0]);
      g_free (str_array);
    }
  g_variant_unref (variant);

  return value;
}

static void
binding_changed (GSettings *settings,
		 const char *key,
		 CcKeyboardItem *item)
{
  char *value;

  g_list_free_full (item->key_combos, g_free);
  item->key_combos = settings_get_key_combos (item->settings, item->key, FALSE);

  value = settings_get_binding (item->settings, item->key);
  item->editable = g_settings_is_writable (item->settings, item->key);
  _set_binding (item, value, FALSE);
  g_free (value);
  g_object_notify (G_OBJECT (item), "binding");
}

gboolean
cc_keyboard_item_load_from_gsettings_path (CcKeyboardItem *item,
                                           const char     *path,
                                           gboolean        reset)
{
  item->schema = g_strdup (CUSTOM_KEYS_SCHEMA);
  item->gsettings_path = g_strdup (path);
  item->key = g_strdup ("binding");
  item->settings = g_settings_new_with_path (item->schema, path);
  item->editable = g_settings_is_writable (item->settings, item->key);
  item->desc_editable = g_settings_is_writable (item->settings, "name");
  item->cmd_editable = g_settings_is_writable (item->settings, "command");

  if (reset)
    {
      g_settings_reset (item->settings, "name");
      g_settings_reset (item->settings, "command");
      g_settings_reset (item->settings, "binding");
    }

  g_settings_bind (item->settings, "name",
                   G_OBJECT (item), "description", G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (item->settings, "command",
                   G_OBJECT (item), "command", G_SETTINGS_BIND_DEFAULT);

  g_list_free_full (item->key_combos, g_free);
  item->key_combos = settings_get_key_combos (item->settings, item->key, FALSE);

  g_free (item->binding);
  item->binding = settings_get_binding (item->settings, item->key);
  binding_from_string (item->binding, item->primary_combo);
  g_signal_connect (G_OBJECT (item->settings), "changed::binding",
		    G_CALLBACK (binding_changed), item);

  return TRUE;
}

gboolean
cc_keyboard_item_load_from_gsettings (CcKeyboardItem *item,
				      const char *description,
				      const char *schema,
				      const char *key)
{
  char *signal_name;

  item->schema = g_strdup (schema);
  item->key = g_strdup (key);
  item->description = g_strdup (description);

  item->settings = g_settings_new (item->schema);
  g_free (item->binding);
  item->binding = settings_get_binding (item->settings, item->key);
  item->editable = g_settings_is_writable (item->settings, item->key);
  binding_from_string (item->binding, item->primary_combo);

  g_list_free_full (item->key_combos, g_free);
  item->key_combos = settings_get_key_combos (item->settings, item->key, FALSE);

  g_list_free_full (item->default_combos, g_free);
  item->default_combos = settings_get_key_combos (item->settings, item->key, TRUE);

  signal_name = g_strdup_printf ("changed::%s", item->key);
  g_signal_connect (G_OBJECT (item->settings), signal_name,
		    G_CALLBACK (binding_changed), item);
  g_free (signal_name);

  return TRUE;
}

gboolean
cc_keyboard_item_equal (CcKeyboardItem *a,
			CcKeyboardItem *b)
{
  if (a->type != b->type)
    return FALSE;
  switch (a->type)
    {
      case CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH:
	return g_str_equal (a->gsettings_path, b->gsettings_path);
      case CC_KEYBOARD_ITEM_TYPE_GSETTINGS:
	return (g_str_equal (a->schema, b->schema) &&
		g_str_equal (a->key, b->key));
      default:
	g_assert_not_reached ();
    }

}

void
cc_keyboard_item_add_reverse_item (CcKeyboardItem *item,
				   CcKeyboardItem *reverse_item,
				   gboolean is_reversed)
{
  g_return_if_fail (item->key != NULL);

  item->reverse_item = reverse_item;
  if (reverse_item->reverse_item == NULL)
    {
      reverse_item->reverse_item = item;
      reverse_item->is_reversed = !is_reversed;
    }
  else
    g_warn_if_fail (reverse_item->is_reversed == !!is_reversed);

  item->is_reversed = !!is_reversed;
}

CcKeyboardItem *
cc_keyboard_item_get_reverse_item (CcKeyboardItem *item)
{
  return item->reverse_item;
}


void
cc_keyboard_item_set_hidden (CcKeyboardItem *item, gboolean hidden)
{
  item->hidden = !!hidden;
}


gboolean
cc_keyboard_item_is_hidden (CcKeyboardItem *item)
{
  return item->hidden;
}

/**
 * cc_keyboard_item_is_value_default:
 * @self: a #CcKeyboardItem
 *
 * Retrieves whether the shortcut is the default value or not.
 *
 * Returns: %TRUE if the shortcut is the default value, %FALSE otherwise.
 */
gboolean
cc_keyboard_item_is_value_default (CcKeyboardItem *self)
{
  GVariant *user_value;
  gboolean is_value_default;

  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (self), FALSE);

  /*
   * When the shortcut is custom, we don't treat it as modified
   * since we don't know what would be its default value.
   */
  if (self->type == CC_KEYBOARD_ITEM_TYPE_GSETTINGS_PATH)
    return TRUE;

  user_value = g_settings_get_user_value (self->settings, self->key);

  is_value_default = TRUE;

  if (user_value)
    {
      GVariant *default_value;
      const gchar *default_binding, *user_binding;

      default_value = g_settings_get_default_value (self->settings, self->key);

      default_binding = get_binding_from_variant (default_value);
      user_binding = get_binding_from_variant (user_value);

      is_value_default = (g_strcmp0 (default_binding, user_binding) == 0);

      g_clear_pointer (&default_value, g_variant_unref);
    }

  g_clear_pointer (&user_value, g_variant_unref);

  return is_value_default;
}

/**
 * cc_keyboard_item_reset:
 * @self: a #CcKeyboardItem
 *
 * Reset the keyboard binding to the default value.
 */
void
cc_keyboard_item_reset (CcKeyboardItem *self)
{
  CcKeyboardItem *reverse;

  g_return_if_fail (CC_IS_KEYBOARD_ITEM (self));

  reverse = self->reverse_item;

  g_settings_reset (self->settings, self->key);
  g_object_notify (G_OBJECT (self), "is-value-default");

  /* Also reset the reverse item */
  if (reverse)
    {
      g_settings_reset (reverse->settings, reverse->key);
      g_object_notify (G_OBJECT (reverse), "is-value-default");
    }
}

GList *
cc_keyboard_item_get_key_combos (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), NULL);
  return item->key_combos;
}

GList *
cc_keyboard_item_get_default_combos (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), NULL);
  return item->default_combos;
}

CcKeyCombo *
cc_keyboard_item_get_primary_combo (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), NULL);
  return item->primary_combo;
}

const gchar *
cc_keyboard_item_get_key (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), NULL);
  return item->key;
}

CcKeyboardItemType
cc_keyboard_item_get_item_type (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), CC_KEYBOARD_ITEM_TYPE_NONE);
  return item->type;
}

void
cc_keyboard_item_set_model (CcKeyboardItem *item, GtkTreeModel *model, BindingGroupType group)
{
  g_return_if_fail (CC_IS_KEYBOARD_ITEM (item));
  item->model = model;
  item->group = group;
}

const gchar *
cc_keyboard_item_get_gsettings_path (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), NULL);
  return item->gsettings_path;
}

GSettings *
cc_keyboard_item_get_settings (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), NULL);
  return item->settings;
}

