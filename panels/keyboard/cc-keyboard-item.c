/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Red Hat, Inc.
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
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib/gi18n-lib.h>

#include <gconf/gconf-client.h>

#include "cc-keyboard-item.h"
#include "eggaccelerators.h"

#define CC_KEYBOARD_ITEM_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_KEYBOARD_ITEM, CcKeyboardItemPrivate))

struct CcKeyboardItemPrivate
{
  /* properties */
  int foo;

  /* internal */
};

enum {
  PROP_0,
  PROP_DESCRIPTION,
  PROP_BINDING,
  PROP_EDITABLE,
  PROP_TYPE,
  PROP_COMMAND
};

static void     cc_keyboard_item_class_init     (CcKeyboardItemClass *klass);
static void     cc_keyboard_item_init           (CcKeyboardItem      *keyboard_item);
static void     cc_keyboard_item_finalize       (GObject               *object);

G_DEFINE_TYPE (CcKeyboardItem, cc_keyboard_item, G_TYPE_OBJECT)

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

static void
_set_binding (CcKeyboardItem *item,
              const char     *value,
	      gboolean        set_backend)
{
  g_free (item->binding);
  item->binding = g_strdup (value);
  binding_from_string (item->binding, &item->keyval, &item->keycode, &item->mask);

  if (set_backend == FALSE)
    return;

  if (item->type == CC_KEYBOARD_ITEM_TYPE_GCONF ||
      item->type == CC_KEYBOARD_ITEM_TYPE_GCONF_DIR)
    {
      GConfClient *client;
      GError *err = NULL;
      const char *key;

      client = gconf_client_get_default ();
      if (item->type == CC_KEYBOARD_ITEM_TYPE_GCONF)
	key = item->gconf_key;
      else
	key = item->binding_gconf_key;
      gconf_client_set_string (client, key, item->binding, &err);
      if (err != NULL)
        {
	  g_warning ("Failed to set '%s' to '%s': %s", key, item->binding, err->message);
	  g_error_free (err);
	}
    }
  else if (item->type == CC_KEYBOARD_ITEM_TYPE_GSETTINGS)
    {
      g_settings_set_string (item->settings, item->key, item->binding);
    }
}

const char *
cc_keyboard_item_get_binding (CcKeyboardItem *item)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_ITEM (item), NULL);

  return item->binding;
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
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    break;
  }
}

static GObject *
cc_keyboard_item_constructor (GType                  type,
                              guint                  n_construct_properties,
                              GObjectConstructParam *construct_properties)
{
  CcKeyboardItem      *keyboard_item;

  keyboard_item = CC_KEYBOARD_ITEM (G_OBJECT_CLASS (cc_keyboard_item_parent_class)->constructor (type,
                                                                                                 n_construct_properties,
                                                                                                 construct_properties));

  return G_OBJECT (keyboard_item);
}

static void
cc_keyboard_item_class_init (CcKeyboardItemClass *klass)
{
  GObjectClass  *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = cc_keyboard_item_get_property;
  object_class->set_property = cc_keyboard_item_set_property;
  object_class->constructor = cc_keyboard_item_constructor;
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

  g_type_class_add_private (klass, sizeof (CcKeyboardItemPrivate));
}

static void
cc_keyboard_item_init (CcKeyboardItem *item)
{
  item->priv = CC_KEYBOARD_ITEM_GET_PRIVATE (item);
}

static void
cc_keyboard_item_finalize (GObject *object)
{
  CcKeyboardItem *item;
  GConfClient *client;

  g_return_if_fail (object != NULL);
  g_return_if_fail (CC_IS_KEYBOARD_ITEM (object));

  item = CC_KEYBOARD_ITEM (object);

  g_return_if_fail (item->priv != NULL);

  /* Remove GConf watches */
  client = gconf_client_get_default ();

  /* FIXME what if we didn't add a watch? */
  if (item->gconf_key_dir != NULL)
    gconf_client_remove_dir (client, item->gconf_key_dir, NULL);
  else if (item->gconf_key != NULL)
    gconf_client_remove_dir (client, item->gconf_key, NULL);

  if (item->gconf_cnxn != 0)
    gconf_client_notify_remove (client, item->gconf_cnxn);
  if (item->gconf_cnxn_desc != 0)
    gconf_client_notify_remove (client, item->gconf_cnxn_desc);
  if (item->gconf_cnxn_cmd != 0)
    gconf_client_notify_remove (client, item->gconf_cnxn_cmd);
  if (item->settings != NULL)
    g_object_unref (item->settings);

  g_object_unref (client);

  /* Free memory */
  g_free (item->binding);
  g_free (item->gettext_package);
  g_free (item->gconf_key);
  g_free (item->gconf_key_dir);
  g_free (item->binding_gconf_key);
  g_free (item->description);
  g_free (item->desc_gconf_key);
  g_free (item->command);
  g_free (item->cmd_gconf_key);
  g_free (item->schema);
  g_free (item->key);

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

static void
keybinding_description_changed (GConfClient    *client,
                                guint           cnxn_id,
                                GConfEntry     *entry,
                                CcKeyboardItem *item)
{
  const gchar *key_value;

  key_value = entry->value ? gconf_value_get_string (entry->value) : NULL;
  _set_description (item, key_value);
  item->desc_editable = gconf_entry_get_is_writable (entry);
  g_object_notify (G_OBJECT (item), "description");
}

static void
keybinding_key_changed (GConfClient    *client,
                        guint           cnxn_id,
                        GConfEntry     *entry,
                        CcKeyboardItem *item)
{
  const gchar *key_value;

  key_value = entry->value ? gconf_value_get_string (entry->value) : NULL;
  _set_binding (item, key_value, FALSE);
  item->editable = gconf_entry_get_is_writable (entry);
  g_object_notify (G_OBJECT (item), "binding");
}

static void
keybinding_command_changed (GConfClient    *client,
                            guint           cnxn_id,
                            GConfEntry     *entry,
                            CcKeyboardItem *item)
{
  const gchar *key_value;

  key_value = entry->value ? gconf_value_get_string (entry->value) : NULL;
  _set_command (item, key_value);
  item->cmd_editable = gconf_entry_get_is_writable (entry);
  g_object_notify (G_OBJECT (item), "command");
}

static void
binding_changed (GSettings *settings,
		 const char *key,
		 CcKeyboardItem *item)
{
  char *value;

  value = g_settings_get_string (item->settings, item->key);
  item->editable = g_settings_is_writable (item->settings, item->key);
  _set_binding (item, value, FALSE);
  g_object_notify (G_OBJECT (item), "binding");
}

gboolean
cc_keyboard_item_load_from_gconf (CcKeyboardItem *item,
				  const char *gettext_package,
                                  const char *key)
{
  GConfClient *client;
  GConfEntry *entry;

  client = gconf_client_get_default ();

  item->gconf_key = g_strdup (key);
  entry = gconf_client_get_entry (client,
                                  item->gconf_key,
                                  NULL,
                                  TRUE,
                                  NULL);
  if (entry == NULL) {
    g_object_unref (client);
    return FALSE;
  }

  if (gconf_entry_get_schema_name (entry)) {
    GConfSchema *schema;
    const char *description;

    schema = gconf_client_get_schema (client,
                                      gconf_entry_get_schema_name (entry),
                                      NULL);
    if (schema != NULL) {
      if (gettext_package != NULL) {
	bind_textdomain_codeset (gettext_package, "UTF-8");
	description = dgettext (gettext_package, gconf_schema_get_short_desc (schema));
      } else {
	description = _(gconf_schema_get_short_desc (schema));
      }
      item->description = g_strdup (description);
      gconf_schema_free (schema);
    }
  }
  if (item->description == NULL) {
    /* Only print a warning for keys that should have a schema */
    g_warning ("No description for key '%s'", item->gconf_key);
  }
  item->editable = gconf_entry_get_is_writable (entry);
  gconf_client_add_dir (client, item->gconf_key, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);
  item->gconf_cnxn = gconf_client_notify_add (client,
                                              item->gconf_key,
                                              (GConfClientNotifyFunc) &keybinding_key_changed,
                                              item, NULL, NULL);
  item->binding = gconf_client_get_string (client, item->gconf_key, NULL);
  binding_from_string (item->binding, &item->keyval, &item->keycode, &item->mask);

  gconf_entry_free (entry);
  g_object_unref (client);

  return TRUE;
}

gboolean
cc_keyboard_item_load_from_gconf_dir (CcKeyboardItem *item,
                                      const char *key_dir)
{
  GConfClient *client;
  GConfEntry *entry;

  /* FIXME add guards:
   * key_dir finishing with '/' */

  client = gconf_client_get_default ();

  item->gconf_key_dir = g_strdup (key_dir);
  item->binding_gconf_key = g_strdup_printf ("%s/binding", item->gconf_key_dir);
  item->desc_gconf_key = g_strdup_printf ("%s/name", item->gconf_key_dir);
  item->cmd_gconf_key = g_strdup_printf ("%s/action", item->gconf_key_dir);
  item->description = gconf_client_get_string (client, item->desc_gconf_key, NULL);

  entry = gconf_client_get_entry (client,
                                  item->binding_gconf_key,
                                  NULL,
                                  TRUE,
                                  NULL);
  if (entry == NULL)
    return FALSE;

  item->command = gconf_client_get_string (client, item->cmd_gconf_key, NULL);
  item->editable = gconf_entry_get_is_writable (entry);
  gconf_client_add_dir (client, item->gconf_key_dir, GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

  item->desc_editable = gconf_client_key_is_writable (client, item->desc_gconf_key, NULL);
  item->gconf_cnxn_desc = gconf_client_notify_add (client,
                                                   item->desc_gconf_key,
                                                   (GConfClientNotifyFunc) &keybinding_description_changed,
                                                   item, NULL, NULL);

  item->cmd_editable = gconf_client_key_is_writable (client, item->cmd_gconf_key, NULL);
  item->gconf_cnxn_cmd = gconf_client_notify_add (client,
                                                  item->cmd_gconf_key,
                                                  (GConfClientNotifyFunc) &keybinding_command_changed,
                                                  item, NULL, NULL);

  item->cmd_editable = gconf_client_key_is_writable (client, item->binding_gconf_key, NULL);
  item->gconf_cnxn = gconf_client_notify_add (client,
                                              item->binding_gconf_key,
                                              (GConfClientNotifyFunc) &keybinding_key_changed,
                                              item, NULL, NULL);

  item->binding = gconf_client_get_string (client, item->binding_gconf_key, NULL);
  binding_from_string (item->binding, &item->keyval, &item->keycode, &item->mask);

  gconf_entry_free (entry);
  g_object_unref (client);

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
  item->binding = g_settings_get_string (item->settings, item->key);
  item->editable = g_settings_is_writable (item->settings, item->key);
  binding_from_string (item->binding, &item->keyval, &item->keycode, &item->mask);

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
      case CC_KEYBOARD_ITEM_TYPE_GCONF:
        return g_str_equal (a->gconf_key, b->gconf_key);
      case CC_KEYBOARD_ITEM_TYPE_GCONF_DIR:
	return g_str_equal (a->gconf_key_dir, b->gconf_key_dir);
      case CC_KEYBOARD_ITEM_TYPE_GSETTINGS:
	return (g_str_equal (a->schema, b->schema) &&
		g_str_equal (a->key, b->key));
      default:
	g_assert_not_reached ();
    }

}

/*
 * vim: sw=2 ts=8 cindent noai bs=2
 */
