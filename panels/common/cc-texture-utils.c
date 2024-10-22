/* Copyright (C) 2016 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/* Adapted from gdktextureutils.c
 * https://gitlab.gnome.org/GNOME/gtk/-/blob/bef6352401561d71756326f50c3f223655b3d16e/gtk/gdktextureutils.c
 */

#include "config.h"

#include "cc-texture-utils.h"

#include "cc-scaler.h"

typedef struct {
  double scale;
} LoaderData;

static void
on_loader_size_prepared (GdkPixbufLoader *loader,
                         int              width,
                         int              height,
                         gpointer         user_data)
{
  LoaderData *loader_data = user_data;
  GdkPixbufFormat *format;

  /* Let the regular icon helper code path handle non-scalable images */
  format = gdk_pixbuf_loader_get_format (loader);
  if (!gdk_pixbuf_format_is_scalable (format))
    {
      loader_data->scale = 1.0;
      return;
    }

  gdk_pixbuf_loader_set_size (loader,
                              width * loader_data->scale,
                              height * loader_data->scale);
}

static GdkPaintable *
cc_texture_new_from_bytes_scaled (GBytes *bytes,
                                  double  scale)
{
  LoaderData loader_data;
  g_autoptr (GdkTexture) texture = NULL;
  g_autoptr (GdkPaintable) paintable = NULL;
  g_autoptr (GdkPixbufLoader) loader = NULL;
  gboolean success;

  loader_data.scale = scale;

  loader = gdk_pixbuf_loader_new ();
  g_signal_connect (loader, "size-prepared",
                    G_CALLBACK (on_loader_size_prepared), &loader_data);

  success = gdk_pixbuf_loader_write_bytes (loader, bytes, NULL);
  /* close even when writing failed */
  success &= gdk_pixbuf_loader_close (loader, NULL);

  if (!success)
    return NULL;

  texture = gdk_texture_new_for_pixbuf (gdk_pixbuf_loader_get_pixbuf (loader));

  if (loader_data.scale != 1.0)
    return cc_scaler_new (GDK_PAINTABLE (texture), loader_data.scale);
  else
    return GDK_PAINTABLE (g_steal_pointer (&texture));
}

GdkPaintable *
cc_texture_new_from_resource_scaled (const char *path,
                                     double      scale)
{
  g_autoptr (GBytes) bytes = NULL;

  bytes = g_resources_lookup_data (path, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
  if (!bytes)
    return NULL;

  return cc_texture_new_from_bytes_scaled (bytes, scale);
}
