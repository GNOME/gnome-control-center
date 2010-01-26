/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 William Jon McCann
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 */

#ifndef __CC_THEME_THUMBNAILER_H
#define __CC_THEME_THUMBNAILER_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "gnome-theme-info.h"

G_BEGIN_DECLS

#define CC_TYPE_THEME_THUMBNAILER         (cc_theme_thumbnailer_get_type ())
#define CC_THEME_THUMBNAILER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CC_TYPE_THEME_THUMBNAILER, CcThemeThumbnailer))
#define CC_THEME_THUMBNAILER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CC_TYPE_THEME_THUMBNAILER, CcThemeThumbnailerClass))
#define CC_IS_THEME_THUMBNAILER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CC_TYPE_THEME_THUMBNAILER))
#define CC_IS_THEME_THUMBNAILER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CC_TYPE_THEME_THUMBNAILER))
#define CC_THEME_THUMBNAILER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CC_TYPE_THEME_THUMBNAILER, CcThemeThumbnailerClass))

typedef struct CcThemeThumbnailerPrivate CcThemeThumbnailerPrivate;

typedef struct
{
        GObject                   parent;
        CcThemeThumbnailerPrivate *priv;
} CcThemeThumbnailer;

typedef struct
{
        GObjectClass   parent_class;
} CcThemeThumbnailerClass;

typedef enum
{
         CC_THEME_THUMBNAILER_ERROR_GENERAL
} CcThemeThumbnailerError;

#define CC_THEME_THUMBNAILER_ERROR cc_theme_thumbnailer_error_quark ()

typedef void (* CcThemeThumbnailFunc)          (GdkPixbuf          *pixbuf,
                                                gchar              *theme_name,
                                                gpointer            data);

GQuark                cc_theme_thumbnailer_error_quark                    (void);
GType                 cc_theme_thumbnailer_get_type                       (void);

CcThemeThumbnailer *   cc_theme_thumbnailer_new                            (void);


void                  cc_theme_thumbnailer_create_meta_async     (CcThemeThumbnailer  *thumbnailer,
                                                                  GnomeThemeMetaInfo  *theme_info,
                                                                  CcThemeThumbnailFunc func,
                                                                  gpointer             data,
                                                                  GDestroyNotify       destroy);
void                  cc_theme_thumbnailer_create_gtk_async      (CcThemeThumbnailer  *thumbnailer,
                                                                  GnomeThemeInfo      *theme_info,
                                                                  CcThemeThumbnailFunc func,
                                                                  gpointer             data,
                                                                  GDestroyNotify       destroy);
void                  cc_theme_thumbnailer_create_metacity_async (CcThemeThumbnailer  *thumbnailer,
                                                                  GnomeThemeInfo      *theme_info,
                                                                  CcThemeThumbnailFunc func,
                                                                  gpointer             data,
                                                                  GDestroyNotify       destroy);
void                  cc_theme_thumbnailer_create_icon_async     (CcThemeThumbnailer  *thumbnailer,
                                                                  GnomeThemeIconInfo  *theme_info,
                                                                  CcThemeThumbnailFunc func,
                                                                  gpointer             data,
                                                                  GDestroyNotify       destroy);

G_END_DECLS

#endif /* __CC_THEME_THUMBNAILER_H */
