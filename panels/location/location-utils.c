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
  g_variant_location_print (val);
  return val;
}

GVariant *
g_variant_array_add_value (GVariant *container,
                           GVariant *value)
{
  g_debug ("Adding value to the locations array");

  GVariantBuilder *builder = g_variant_builder_new (G_VARIANT_TYPE_ARRAY);
  GVariantIter iter;
  GVariant *val;

  g_variant_iter_init (&iter, container);
  g_debug ("About to loop in the previous locations");
  while ((val = g_variant_iter_next_value (&iter))) {
    g_debug ("In the loop\n");
    g_variant_location_print (val);
    g_variant_builder_add_value (builder, val);
  }
  g_debug ("Added the previous locations");
  g_variant_builder_add_value (builder, value);

  return g_variant_builder_end (builder);
}

void
g_variant_location_print (GVariant *location)
{
  g_print("Location:\n");
  g_print ("\tcity:%s\n"
           "\tcountry: %s\n"
           "\ttimezone: %i\n"
           "\tlong: %f\n"
           "\tlat: %f\n",
           g_variant_get_string (g_variant_lookup_value (location,
                                                         "city",
                                                         G_VARIANT_TYPE_STRING),
                                 NULL),
           g_variant_get_string (g_variant_lookup_value (location,
                                                         "country",
                                                         G_VARIANT_TYPE_STRING),
                                 NULL),
           g_variant_get_int16 (g_variant_lookup_value (location,
                                                        "timezone",
                                                        G_VARIANT_TYPE_INT16)),
           g_variant_get_double (g_variant_lookup_value (location,
                                                         "latitude",
                                                         G_VARIANT_TYPE_DOUBLE)),
           g_variant_get_double (g_variant_lookup_value (location,
                                                         "longitude",
                                                         G_VARIANT_TYPE_DOUBLE))
           );

}
