/*
 * Copyright © 2018 Canonical Ltd.
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
 */

#include <config.h>
#include "cc-input-source.h"

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

enum
{
  SIGNAL_LABEL_CHANGED,
  SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = {0};

G_DEFINE_TYPE (CcInputSource, cc_input_source, G_TYPE_OBJECT)

void
cc_input_source_class_init (CcInputSourceClass *klass)
{
  signals[SIGNAL_LABEL_CHANGED] =
    g_signal_new ("label-changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE,
                  0);
}

void
cc_input_source_init (CcInputSource *source)
{
}

void
cc_input_source_emit_label_changed (CcInputSource *source)
{
  g_return_if_fail (CC_IS_INPUT_SOURCE (source));
  g_signal_emit (source, signals[SIGNAL_LABEL_CHANGED], 0);
}

gchar *
cc_input_source_get_label (CcInputSource *source)
{
  g_return_val_if_fail (CC_IS_INPUT_SOURCE (source), NULL);
  return CC_INPUT_SOURCE_GET_CLASS (source)->get_label (source);
}

gboolean
cc_input_source_matches (CcInputSource *source,
                         CcInputSource *source2)
{
  g_return_val_if_fail (CC_IS_INPUT_SOURCE (source), FALSE);
  return CC_INPUT_SOURCE_GET_CLASS (source)->matches (source, source2);
}

const gchar *
cc_input_source_get_layout (CcInputSource *source)
{
  g_return_val_if_fail (CC_IS_INPUT_SOURCE (source), NULL);
  return CC_INPUT_SOURCE_GET_CLASS (source)->get_layout (source);
}

const gchar *
cc_input_source_get_layout_variant (CcInputSource *source)
{
  g_return_val_if_fail (CC_IS_INPUT_SOURCE (source), NULL);
  return CC_INPUT_SOURCE_GET_CLASS (source)->get_layout_variant (source);
}

static void
launch_viewer (CcInputSource *source,
               const gchar   *handle)
{
  const gchar *layout, *layout_variant;
  g_autoptr(GPtrArray) argv = NULL;
  g_autofree gchar *layout_desc = NULL;

  argv = g_ptr_array_new ();
  g_ptr_array_add (argv, KEYBOARD_PREVIEWER_EXEC);

  if (handle)
    {
      g_ptr_array_add (argv, "--parent-handle");
      g_ptr_array_add (argv, (gpointer) handle);
    }

  layout = cc_input_source_get_layout (source);
  layout_variant = cc_input_source_get_layout_variant (source);

  if (layout_variant && layout_variant[0])
    layout_desc = g_strdup_printf ("%s+%s", layout, layout_variant);
  else
    layout_desc = g_strdup_printf ("%s", layout);

  g_ptr_array_add (argv, layout_desc);
  g_ptr_array_add (argv, NULL);

  g_debug ("Launching keyboard previewer with layout: '%s'\n", layout_desc);
  g_spawn_async (NULL, (gchar **) argv->pdata, NULL,
                 G_SPAWN_DEFAULT, NULL, NULL, NULL, NULL);
}

#ifdef GDK_WINDOWING_WAYLAND
static void
toplevel_handle_exported (GdkToplevel *toplevel,
                          const gchar *handle,
                          gpointer     user_data)
{
  CcInputSource *source = user_data;

  launch_viewer (source, handle);
}
#endif

void
cc_input_source_launch_previewer (CcInputSource *source,
                                  GtkWidget     *requester)
{
  GdkDisplay *display G_GNUC_UNUSED;

  g_return_if_fail (CC_IS_INPUT_SOURCE (source));

#ifdef GDK_WINDOWING_WAYLAND
  display = gtk_widget_get_display (GTK_WIDGET (requester));

  if (GDK_IS_WAYLAND_DISPLAY (display))
    {
      GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (requester));
      GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (root));

      gdk_wayland_toplevel_export_handle (GDK_TOPLEVEL (surface),
                                          toplevel_handle_exported,
                                          source,
                                          NULL);
    }
  else
#endif
    {
      launch_viewer (source, NULL);
    }
}
