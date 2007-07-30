#ifndef __THEME_THUMBNAIL_H__
#define __THEME_THUMBNAIL_H__


#include <gtk/gtk.h>
#include "gnome-theme-info.h"

typedef void (* ThemeThumbnailFunc)          (GdkPixbuf          *pixbuf,
                                              gchar              *theme_name,
                                              gpointer            data);

GdkPixbuf *generate_meta_theme_thumbnail     (GnomeThemeMetaInfo *theme_info);
GdkPixbuf *generate_gtk_theme_thumbnail      (GnomeThemeInfo     *theme_info);
GdkPixbuf *generate_metacity_theme_thumbnail (GnomeThemeInfo     *theme_info);
GdkPixbuf *generate_icon_theme_thumbnail     (GnomeThemeIconInfo *theme_info);

void generate_meta_theme_thumbnail_async     (GnomeThemeMetaInfo *theme_info,
                                              ThemeThumbnailFunc  func,
                                              gpointer            data,
                                              GDestroyNotify      destroy);
void generate_gtk_theme_thumbnail_async      (GnomeThemeInfo     *theme_info,
                                              ThemeThumbnailFunc  func,
                                              gpointer            data,
                                              GDestroyNotify      destroy);
void generate_metacity_theme_thumbnail_async (GnomeThemeInfo     *theme_info,
                                              ThemeThumbnailFunc  func,
                                              gpointer            data,
                                              GDestroyNotify      destroy);
void generate_icon_theme_thumbnail_async     (GnomeThemeIconInfo *theme_info,
                                              ThemeThumbnailFunc  func,
                                              gpointer            data,
                                              GDestroyNotify      destroy);

void theme_thumbnail_factory_init            (int                 argc,
                                              char               *argv[]);

#endif /* __THEME_THUMBNAIL_H__ */
