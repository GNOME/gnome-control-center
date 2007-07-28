/*
 * Copyright (C) 2007 The GNOME Foundation
 * Written by Denis Washington <denisw@svn.gnome.org>
 *            Jens Granseuer <jensgr@gmx.net>
 * All Rights Reserved
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include "wp-cellrenderer.h"

static void cell_renderer_wallpaper_render (GtkCellRenderer *cell,
                                            GdkWindow *window,
                                            GtkWidget *widget,
                                            GdkRectangle *background_area,
                                            GdkRectangle *cell_area,
                                            GdkRectangle *expose_area,
                                            GtkCellRendererState flags);

G_DEFINE_TYPE (CellRendererWallpaper, cell_renderer_wallpaper, GTK_TYPE_CELL_RENDERER_PIXBUF)

static void
cell_renderer_wallpaper_class_init (CellRendererWallpaperClass *klass)
{
  GtkCellRendererClass *renderer_class;

  renderer_class = (GtkCellRendererClass *) klass;
  renderer_class->render = cell_renderer_wallpaper_render;
}

static void
cell_renderer_wallpaper_init (CellRendererWallpaper *renderer)
{

}

GtkCellRenderer *
cell_renderer_wallpaper_new (void)
{
  return g_object_new (cell_renderer_wallpaper_get_type (), NULL);
}

static void
cell_renderer_wallpaper_render (GtkCellRenderer *cell,
                                GdkWindow *window,
                                GtkWidget *widget,
                                GdkRectangle *background_area,
                                GdkRectangle *cell_area,
                                GdkRectangle *expose_area,
                                GtkCellRendererState flags)
{
  (* GTK_CELL_RENDERER_CLASS (cell_renderer_wallpaper_parent_class)->render)
      (cell, window, widget, background_area, cell_area, expose_area, flags);

  if ((flags & (GTK_CELL_RENDERER_SELECTED|GTK_CELL_RENDERER_PRELIT)) != 0)
  {
    GtkStateType state;
    GdkGC *gc;

    if ((flags & GTK_CELL_RENDERER_SELECTED) != 0)
    {
      if (GTK_WIDGET_HAS_FOCUS (widget))
        state = GTK_STATE_SELECTED;
      else
        state = GTK_STATE_ACTIVE;
    }
    else
      state = GTK_STATE_PRELIGHT;

    gc = gdk_gc_new (GDK_DRAWABLE (window));
    gdk_gc_set_foreground (gc, &widget->style->bg[state]);
    gdk_gc_set_line_attributes (gc, 3,
                                GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);

    gdk_draw_rectangle (GDK_DRAWABLE (window), gc, FALSE,
                        background_area->x, background_area->y,
                        background_area->width, background_area->height);
  }
}
