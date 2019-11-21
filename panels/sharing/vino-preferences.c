/*
 * Copyright (C) 2003 Sun Microsystems, Inc.
 * Copyright (C) 2006 Jonh Wendell <wendell@bani.com.br> 
 * Copyright © 2010 Codethink Limited
 * Copyright (C) 2013 Intel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *      Mark McLoughlin <mark@skynet.ie>
 *      Jonh Wendell <wendell@bani.com.br>
 *      Ryan Lortie <desrt@desrt.ca>
 */

#include "vino-preferences.h"
#include <string.h>

#include <glib/gi18n.h>

gboolean
vino_get_authtype (GValue   *value,
                   GVariant *variant,
                   gpointer  user_data)
{
  GVariantIter iter;
  const gchar *type;

  g_variant_iter_init (&iter, variant);
  g_value_set_boolean (value, TRUE);

  while (g_variant_iter_next (&iter, "&s", &type))
    if (strcmp (type, "none") == 0)
      g_value_set_boolean (value, FALSE);

  return TRUE;
}

GVariant *
vino_set_authtype (const GValue       *value,
                   const GVariantType *type,
                   gpointer            user_data)
{
  const gchar *authtype;

  if (g_value_get_boolean (value))
    authtype = "vnc";
  else
    authtype = "none";

  return g_variant_new_strv (&authtype, 1);
}

gboolean
vino_get_password (GValue   *value,
                   GVariant *variant,
                   gpointer  user_data)
{
  const gchar *setting;

  setting = g_variant_get_string (variant, NULL);

  if (strcmp (setting, "keyring") == 0)
    {
      /* "keyring" is the default value, even though vino doesn't support it at
       * the moment */

      g_value_set_static_string (value, "");
      return TRUE;
    }
  else
    {
      gchar *decoded;
      gsize length;
      gboolean ok;

      decoded = (gchar *) g_base64_decode (setting, &length);

      if ((ok = g_utf8_validate (decoded, length, NULL)))
        g_value_take_string (value, g_strndup (decoded, length));

      return ok;
    }
}

GVariant *
vino_set_password (const GValue       *value,
                   const GVariantType *type,
                   gpointer            user_data)
{
  const gchar *string;
  gchar *base64;

  string = g_value_get_string (value);

  base64 = g_base64_encode ((guchar *) string, strlen (string));
  return g_variant_new_from_data (G_VARIANT_TYPE_STRING,
                                  base64, strlen (base64) + 1,
                                  TRUE, g_free, base64);
}

