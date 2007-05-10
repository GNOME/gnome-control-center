#include <gtk/gtk.h>

#ifndef _WP_CELL_RENDERER_H
#define _WP_CELL_RENDERER_H

G_BEGIN_DECLS

typedef struct _CellRendererWallpaper       CellRendererWallpaper;
typedef struct _CellRendererWallpaperClass  CellRendererWallpaperClass;

struct _CellRendererWallpaper
{
	GtkCellRendererPixbuf parent;
};

struct _CellRendererWallpaperClass
{
	GtkCellRendererPixbufClass parent;
};

GtkCellRenderer *cell_renderer_wallpaper_new (void);

G_END_DECLS

#endif
