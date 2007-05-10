
#include "wp-cellrenderer.h"
#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

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
  
  if (flags && GTK_CELL_RENDERER_FOCUSED)
  {
    GdkGC *gc;

    gc = gdk_gc_new (GDK_DRAWABLE (window));
    
    gdk_gc_set_foreground (gc, &widget->style->bg[GTK_STATE_SELECTED]);
    gdk_gc_set_line_attributes (gc, 3,
                                GDK_LINE_SOLID, GDK_CAP_BUTT, GDK_JOIN_MITER);
    
    gdk_draw_rectangle (GDK_DRAWABLE (window), gc, FALSE,
                        background_area->x, background_area->y,
                        background_area->width, background_area->height);
  }
}
