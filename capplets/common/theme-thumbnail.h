#ifndef __THEME_THUMBNAIL_H__
#define __THEME_THUMBNAIL_H__


#include <gtk/gtk.h>
#include "gnome-theme-info.h"

typedef void (* ThemeThumbnailFunc) (GdkPixbuf          *pixbuf,
				     gpointer            data);


GdkPixbuf *generate_theme_thumbnail         (GnomeThemeMetaInfo *meta_theme_info,
					     gboolean            clear_cache);
void       generate_theme_thumbnail_async   (GnomeThemeMetaInfo *meta_theme_info,
					     gboolean            clear_cache,
					     ThemeThumbnailFunc  func,
					     gpointer            data,
					     GDestroyNotify      destroy);
void       theme_thumbnail_invalidate_cache (GnomeThemeMetaInfo *meta_theme_info);
void       theme_thumbnail_factory_init     (int                 argc,
					     char               *argv[]);

/* Functions for specific types of themes */
GdkPixbuf *generate_gtk_theme_thumbnail      (GnomeThemeInfo *theme_info);
GdkPixbuf *generate_metacity_theme_thumbnail (GnomeThemeInfo *theme_info);
GdkPixbuf *generate_icon_theme_thumbnail     (GnomeThemeIconInfo *theme_info);


#endif /* __THEME_THUMBNAIL_H__ */
