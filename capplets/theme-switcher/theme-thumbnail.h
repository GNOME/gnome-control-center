#ifndef __THEME_THUMBNAIL_H__
#define __THEME_THUMBNAIL_H__


#include <gtk/gtk.h>
#include "gnome-theme-info.h"

GdkPixbuf *generate_theme_thumbnail      (GnomeThemeMetaInfo *meta_theme_info);
void       setup_theme_thumbnail_factory (int                 argc,
					  char               *argv[]);

#endif /* __THEME_THUMBNAIL_H__ */
