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
  g_debug ("Creating a new GVariant");
  GVariantBuilder *builder = g_variant_builder_new (G_VARIANT_TYPE_DICTIONARY);

  g_variant_builder_add_parsed (builder, "{'city', <%s>}", city);
  g_variant_builder_add_parsed (builder, "{'country', <%s>}", country);
  g_variant_builder_add_parsed (builder, "{'timezone', <%n>}", timezone);
  g_variant_builder_add_parsed (builder, "{'longitude', <%d>}", longitude);
  g_variant_builder_add_parsed (builder, "{'latitude', <%d>}", latitude);

  GVariant *val = g_variant_builder_end (builder);
  return val;
}

GVariant *
g_variant_array_add_value (GVariant *container,
                           GVariant *value)
{
  GVariantBuilder *builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
  GVariantIter iter;
  GVariant *val;

  g_variant_iter_init (&iter, container);
  while ((val = g_variant_iter_next_value (&iter))) {
    g_variant_builder_add_value (builder, val);
  }
  g_variant_builder_add_value (builder, value);

  GVariant *res = g_variant_builder_end (builder);
  g_variant_builder_unref (builder);

  g_variant_ref_sink (res);
  return res;
}
