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

#ifndef __CC_THEME_THUMBNAILER_SLAVE_H
#define __CC_THEME_THUMBNAILER_SLAVE_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "gnome-theme-info.h"

G_BEGIN_DECLS

#define CC_TYPE_THEME_THUMBNAILER_SLAVE         (cc_theme_thumbnailer_slave_get_type ())
#define CC_THEME_THUMBNAILER_SLAVE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), CC_TYPE_THEME_THUMBNAILER_SLAVE, CcThemeThumbnailerSlave))
#define CC_THEME_THUMBNAILER_SLAVE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), CC_TYPE_THEME_THUMBNAILER_SLAVE, CcThemeThumbnailerSlaveClass))
#define CC_IS_THEME_THUMBNAILER_SLAVE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), CC_TYPE_THEME_THUMBNAILER_SLAVE))
#define CC_IS_THEME_THUMBNAILER_SLAVE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), CC_TYPE_THEME_THUMBNAILER_SLAVE))
#define CC_THEME_THUMBNAILER_SLAVE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), CC_TYPE_THEME_THUMBNAILER_SLAVE, CcThemeThumbnailerSlaveClass))

typedef struct CcThemeThumbnailerSlavePrivate CcThemeThumbnailerSlavePrivate;

typedef struct
{
        GObject                   parent;
        CcThemeThumbnailerSlavePrivate *priv;
} CcThemeThumbnailerSlave;

typedef struct
{
        GObjectClass   parent_class;
} CcThemeThumbnailerSlaveClass;

typedef enum
{
         CC_THEME_THUMBNAILER_SLAVE_ERROR_GENERAL
} CcThemeThumbnailerSlaveError;

#define CC_THEME_THUMBNAILER_SLAVE_ERROR cc_theme_thumbnailer_slave_error_quark ()

GQuark                cc_theme_thumbnailer_slave_error_quark           (void);
GType                 cc_theme_thumbnailer_slave_get_type              (void);

CcThemeThumbnailerSlave *  cc_theme_thumbnailer_slave_new                   (void);

G_END_DECLS

#endif /* __CC_THEME_THUMBNAILER_SLAVE_H */
