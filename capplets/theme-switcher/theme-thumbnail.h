#ifndef __THEME_THUMBNAIL_H__
#define __THEME_THUMBNAIL_H__


#include <gtk/gtk.h>
#include "gnome-theme-info.h"

typedef void (* ThemeThumbnailFunc) (GdkPixbuf          *pixbuf,
				     gpointer            data);


GdkPixbuf *generate_theme_thumbnail         (GnomeThemeMetaInfo *meta_theme_info);
void       generate_theme_thumbnail_async   (GnomeThemeMetaInfo *meta_theme_info,
					     ThemeThumbnailFunc  func,
					     gpointer            data,
					     GDestroyNotify      destroy);
void       theme_thumbnail_invalidate_cache (GnomeThemeMetaInfo *meta_theme_info);
void       theme_thumbnail_factory_init     (int                 argc,
					     char               *argv[]);



#endif /* __THEME_THUMBNAIL_H__ */
