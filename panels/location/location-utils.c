/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2009-2010  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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

#include "location-utils.h"
#include "config.h"

#include <glib.h>

GVariant *
g_variant_location_new (const char  *city,
			const char  *country,
			const int    timezone,
			const double latitude,
			const double longitude)
{
  GVariantBuilder *builder = g_variant_builder_new (G_VARIANT_TYPE_DICTIONARY);

  GVariant *key = g_variant_new_string ("city");
  GVariant *value = g_variant_new_string (city);
  GVariant *entry = g_variant_new_dict_entry (key, value);
  g_variant_builder_add_value (builder, entry);

  key = g_variant_new_string ("country");
  value = g_variant_new_string (country);
  entry = g_variant_new_dict_entry (key, value);
  g_variant_builder_add_value (builder, entry);

  key = g_variant_new_string ("timezone");
  value = g_variant_new_int16 (timezone);
  entry = g_variant_new_dict_entry (key, value);
  g_variant_builder_add_value (builder, entry);

  key = g_variant_new_string ("longitude");
  value = g_variant_new_double (longitude);
  entry = g_variant_new_dict_entry (key, value);
  g_variant_builder_add_value (builder, entry);

  key = g_variant_new_string ("latitude");
  value = g_variant_new_double (latitude);
  entry = g_variant_new_dict_entry (key, value);
  g_variant_builder_add_value (builder, entry);

  return g_variant_builder_end (builder);
}

GVariant *
g_variant_array_add_value (GVariant *container,
                           GVariant *value)
{
  GVariantBuilder *builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
  GVariantIter iter;
  GVariant *val;

  g_variant_iter_init (&iter, container);
  while (g_variant_iter_loop (&iter, "av", &val))
    g_variant_builder_add_value (builder, val);
  g_variant_builder_add_value (builder,value);

  return g_variant_builder_end (builder);
}
