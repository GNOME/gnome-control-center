/*
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * Written by: Rui Matos <rmatos@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <config.h>
#include <glib/gi18n.h>

#define GNOME_DESKTOP_USE_UNSTABLE_API
#include <libgnome-desktop/gnome-xkb-info.h>

#include "cc-keyboard-option.h"

#define INPUT_SOURCES_SCHEMA "org.gnome.desktop.input-sources"
#define XKB_OPTIONS_KEY "xkb-options"

#define XKB_OPTION_GROUP_LVL3 "lv3"
#define XKB_OPTION_GROUP_COMP "Compose key"
#define XKB_OPTION_GROUP_GRP  "grp"

enum
{
  PROP_0,
  PROP_GROUP,
  PROP_DESCRIPTION
};

enum
{
  CHANGED_SIGNAL,
  LAST_SIGNAL
};

struct _CcKeyboardOption
{
  gchar *group;
  gchar *description;
  gchar *current_value;
  GtkListStore *store;

  const gchar * const *whitelist;
};

G_DEFINE_TYPE (CcKeyboardOption, cc_keyboard_option, G_TYPE_OBJECT);

static guint keyboard_option_signals[LAST_SIGNAL] = { 0 };

static GnomeXkbInfo *xkb_info = NULL;
static GSettings *input_sources_settings = NULL;
static gchar **current_xkb_options = NULL;

static const gchar *xkb_option_lvl3_whitelist[] = {
  "lv3:switch",
  "lv3:menu_switch",
  "lv3:rwin_switch",
  "lv3:lalt_switch",
  "lv3:ralt_switch",
  "lv3:caps_switch",
  "lv3:enter_switch",
  "lv3:bksl_switch",
  "lv3:lsgt_switch",
  NULL
};

static const gchar *xkb_option_comp_whitelist[] = {
  "compose:ralt",
  "compose:rwin",
  "compose:menu",
  "compose:lctrl",
  "compose:rctrl",
  "compose:caps",
  "compose:prsc",
  "compose:sclk",
  NULL
};

/* This list must be kept in sync with what mutter is able to
 * handle. */
static const gchar *xkb_option_grp_whitelist[] = {
  "grp:toggle",
  "grp:lalt_toggle",
  "grp:lwin_toggle",
  "grp:rwin_toggle",
  "grp:lshift_toggle",
  "grp:rshift_toggle",
  "grp:lctrl_toggle",
  "grp:rctrl_toggle",
  "grp:sclk_toggle",
  "grp:menu_toggle",
  "grp:caps_toggle",
  "grp:shift_caps_toggle",
  "grp:alt_caps_toggle",
  "grp:alt_space_toggle",
  "grp:ctrl_shift_toggle",
  "grp:lctrl_lshift_toggle",
  "grp:rctrl_rshift_toggle",
  "grp:ctrl_alt_toggle",
  "grp:alt_shift_toggle",
  "grp:lalt_lshift_toggle",
  NULL
};

static GList *objects_list = NULL;

static gboolean
strv_contains (const gchar * const *strv,
               const gchar         *str)
{
  const gchar * const *p = strv;
  for (p = strv; *p; p++)
    if (g_strcmp0 (*p, str) == 0)
      return TRUE;

  return FALSE;
}

static void
reload_setting (CcKeyboardOption *self)
{
  gchar **iter;

  for (iter = current_xkb_options; *iter; ++iter)
    if (strv_contains (self->whitelist, *iter))
      {
        if (g_strcmp0 (self->current_value, *iter) != 0)
          {
            g_free (self->current_value);
            self->current_value = g_strdup (*iter);
            g_signal_emit (self, keyboard_option_signals[CHANGED_SIGNAL], 0);
          }
        break;
      }

  if (*iter == NULL && self->current_value != NULL)
    {
      g_clear_pointer (&self->current_value, g_free);
      g_signal_emit (self, keyboard_option_signals[CHANGED_SIGNAL], 0);
    }
}

static void
xkb_options_changed (GSettings *settings,
                     gchar     *key,
                     gpointer   data)
{
  GList *l;

  g_strfreev (current_xkb_options);
  current_xkb_options = g_settings_get_strv (settings, key);

  for (l = objects_list; l; l = l->next)
    reload_setting (CC_KEYBOARD_OPTION (l->data));
}

static void
cc_keyboard_option_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  CcKeyboardOption *self;

  self = CC_KEYBOARD_OPTION (object);

  switch (prop_id)
    {
    case PROP_GROUP:
      g_value_set_string (value, self->group);
      break;
    case PROP_DESCRIPTION:
      g_value_set_string (value, self->description);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
cc_keyboard_option_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  CcKeyboardOption *self;

  self = CC_KEYBOARD_OPTION (object);

  switch (prop_id)
    {
    case PROP_GROUP:
      self->group = g_value_dup_string (value);
      break;
    case PROP_DESCRIPTION:
      self->description = g_value_dup_string (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
cc_keyboard_option_init (CcKeyboardOption *self)
{
}

static void
cc_keyboard_option_finalize (GObject *object)
{
  CcKeyboardOption *self = CC_KEYBOARD_OPTION (object);

  g_clear_pointer (&self->group, g_free);
  g_clear_pointer (&self->description, g_free);
  g_clear_pointer (&self->current_value, g_free);
  g_clear_object (&self->store);

  G_OBJECT_CLASS (cc_keyboard_option_parent_class)->finalize (object);
}

static void
cc_keyboard_option_constructed (GObject *object)
{
  GtkTreeIter iter;
  GList *options, *l;
  gchar *option_id;
  CcKeyboardOption *self = CC_KEYBOARD_OPTION (object);

  G_OBJECT_CLASS (cc_keyboard_option_parent_class)->constructed (object);

  if (g_str_equal (self->group, XKB_OPTION_GROUP_LVL3))
    self->whitelist = xkb_option_lvl3_whitelist;
  else if (g_str_equal (self->group, XKB_OPTION_GROUP_COMP))
    self->whitelist = xkb_option_comp_whitelist;
  else if (g_str_equal (self->group, XKB_OPTION_GROUP_GRP))
    self->whitelist = xkb_option_grp_whitelist;
  else
    g_assert_not_reached ();

  self->store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  gtk_list_store_append (self->store, &iter);
  gtk_list_store_set (self->store, &iter,
                      XKB_OPTION_DESCRIPTION_COLUMN, _("Disabled"),
                      XKB_OPTION_ID_COLUMN, NULL,
                      -1);
  options = gnome_xkb_info_get_options_for_group (xkb_info, self->group);
  for (l = options; l; l = l->next)
    {
      option_id = l->data;
      if (strv_contains (self->whitelist, option_id))
        {
          gtk_list_store_append (self->store, &iter);
          gtk_list_store_set (self->store, &iter,
                              XKB_OPTION_DESCRIPTION_COLUMN,
                              gnome_xkb_info_description_for_option (xkb_info, self->group, option_id),
                              XKB_OPTION_ID_COLUMN,
                              option_id,
                              -1);
        }
    }
  g_list_free (options);

  reload_setting (self);
}

static void
cc_keyboard_option_class_init (CcKeyboardOptionClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->get_property = cc_keyboard_option_get_property;
  gobject_class->set_property = cc_keyboard_option_set_property;
  gobject_class->finalize = cc_keyboard_option_finalize;
  gobject_class->constructed = cc_keyboard_option_constructed;

  g_object_class_install_property (gobject_class,
                                   PROP_GROUP,
                                   g_param_spec_string ("group",
                                                        "group",
                                                        "xkb option group identifier",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));
  g_object_class_install_property (gobject_class,
                                   PROP_DESCRIPTION,
                                   g_param_spec_string ("description",
                                                        "description",
                                                        "translated option description",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE));

  keyboard_option_signals[CHANGED_SIGNAL] = g_signal_new ("changed",
                                                          CC_TYPE_KEYBOARD_OPTION,
                                                          G_SIGNAL_RUN_LAST,
                                                          0, NULL, NULL, NULL,
                                                          G_TYPE_NONE,
                                                          0);
}

GList *
cc_keyboard_option_get_all (void)
{
  if (objects_list)
    return objects_list;

  xkb_info = gnome_xkb_info_new ();

  input_sources_settings = g_settings_new (INPUT_SOURCES_SCHEMA);

  g_signal_connect (input_sources_settings, "changed::" XKB_OPTIONS_KEY,
                    G_CALLBACK (xkb_options_changed), NULL);

  xkb_options_changed (input_sources_settings, XKB_OPTIONS_KEY, NULL);

  objects_list = g_list_prepend (objects_list,
                                 g_object_new (CC_TYPE_KEYBOARD_OPTION,
                                               "group", XKB_OPTION_GROUP_LVL3,
                                               /* Translators: This key is also known as 'third level
                                                * chooser'. AltGr is often used for this purpose. See
                                                * https://live.gnome.org/Design/SystemSettings/RegionAndLanguage
                                                */
                                               "description", _("Alternative Characters Key"),
                                               NULL));
  objects_list = g_list_prepend (objects_list,
                                 g_object_new (CC_TYPE_KEYBOARD_OPTION,
                                               "group", XKB_OPTION_GROUP_COMP,
                                              /* Translators: The Compose key is used to initiate key
                                               * sequences that are combined to form a single character.
                                               * See http://en.wikipedia.org/wiki/Compose_key
                                               */
                                               "description", _("Compose Key"),
                                               NULL));
  objects_list = g_list_prepend (objects_list,
                                 g_object_new (CC_TYPE_KEYBOARD_OPTION,
                                               "group", XKB_OPTION_GROUP_GRP,
                                               "description", _("Modifiers-only switch to next source"),
                                               NULL));
  return objects_list;
}

const gchar *
cc_keyboard_option_get_description (CcKeyboardOption *self)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_OPTION (self), NULL);

  return self->description;
}

GtkListStore *
cc_keyboard_option_get_store (CcKeyboardOption *self)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_OPTION (self), NULL);

  return self->store;
}

const gchar *
cc_keyboard_option_get_current_value_description (CcKeyboardOption *self)
{
  g_return_val_if_fail (CC_IS_KEYBOARD_OPTION (self), NULL);

  if (!self->current_value)
    return _("Disabled");

  return gnome_xkb_info_description_for_option (xkb_info, self->group, self->current_value);
}

static void
remove_value (const gchar *value)
{
  gchar **p;

  for (p = current_xkb_options; *p; ++p)
    if (g_str_equal (*p, value))
      {
        g_free (*p);
        break;
      }

  for (++p; *p; ++p)
    *(p - 1) = *p;

  *(p - 1) = NULL;
}

static void
add_value (const gchar *value)
{
  gchar **new_xkb_options;
  gchar **a, **b;

  new_xkb_options = g_new0 (gchar *, g_strv_length (current_xkb_options) + 2);

  a = new_xkb_options;
  for (b = current_xkb_options; *b; ++a, ++b)
    *a = g_strdup (*b);

  *a = g_strdup (value);

  g_strfreev (current_xkb_options);
  current_xkb_options = new_xkb_options;
}

static void
replace_value (const gchar *old,
               const gchar *new)
{
  gchar **iter;

  if (g_str_equal (old, new))
    return;

  for (iter = current_xkb_options; *iter; ++iter)
    if (g_str_equal (*iter, old))
      {
        g_free (*iter);
        *iter = g_strdup (new);
        break;
      }
}

void
cc_keyboard_option_set_selection (CcKeyboardOption *self,
                                  GtkTreeIter      *iter)
{
  g_autofree gchar *new_value = NULL;

  g_return_if_fail (CC_IS_KEYBOARD_OPTION (self));

  gtk_tree_model_get (GTK_TREE_MODEL (self->store), iter,
                      XKB_OPTION_ID_COLUMN, &new_value,
                      -1);

  if (!new_value)
    {
      if (self->current_value)
        remove_value (self->current_value);
    }
  else
    {
      if (self->current_value)
        replace_value (self->current_value, new_value);
      else
        add_value (new_value);
    }

  g_settings_set_strv (input_sources_settings, XKB_OPTIONS_KEY,
                       (const gchar * const *) current_xkb_options);
}

void
cc_keyboard_option_clear_all (void)
{
  GList *l;

  for (l = objects_list; l; l = l->next)
    g_object_unref (l->data);

  g_clear_pointer (&objects_list, g_list_free);
  g_clear_pointer (&current_xkb_options, g_strfreev);
  g_clear_object (&input_sources_settings);
  g_clear_object (&xkb_info);
}
