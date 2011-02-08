/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __NM_ACCESS_POINT_H
#define __NM_ACCESS_POINT_H

#include <glib-object.h>

G_BEGIN_DECLS

#define NM_TYPE_ACCESS_POINT            (nm_access_point_get_type ())
#define NM_ACCESS_POINT(o)              (G_TYPE_CHECK_INSTANCE_CAST ((o), NM_TYPE_ACCESS_POINT, NmAccessPoint))
#define NM_ACCESS_POINT_CLASS(k)        (G_TYPE_CHECK_CLASS_CAST((k), NM_TYPE_ACCESS_POINT, NmAccessPointClass))
#define NM_IS_ACCESS_POINT(o)           (G_TYPE_CHECK_INSTANCE_TYPE ((o), NM_TYPE_ACCESS_POINT))
#define NM_IS_ACCESS_POINT_CLASS(k)     (G_TYPE_CHECK_CLASS_TYPE ((k), NM_TYPE_ACCESS_POINT))
#define NM_ACCESS_POINT_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), NM_TYPE_ACCESS_POINT, NmAccessPointClass))

typedef struct _NmAccessPointPrivate    NmAccessPointPrivate;
typedef struct _NmAccessPoint           NmAccessPoint;
typedef struct _NmAccessPointClass      NmAccessPointClass;

struct _NmAccessPoint
{
         GObject                         parent;
         NmAccessPointPrivate           *priv;
};

struct _NmAccessPointClass
{
        GObjectClass                 parent_class;
        void                        (* changed)                 (NmAccessPoint        *access_point);
};

typedef enum {
        NM_ACCESS_POINT_MODE_UNKNOWN,
        NM_ACCESS_POINT_MODE_ADHOC,
        NM_ACCESS_POINT_MODE_INFRA
} NmAccessPointMode;

GType            nm_access_point_get_type                       (void);
NmAccessPoint   *nm_access_point_new                            (void);

void             nm_access_point_refresh                        (NmAccessPoint          *access_point,
                                                                 const gchar            *object_path,
                                                                 GCancellable           *cancellable);
NmAccessPointMode nm_access_point_get_mode                      (NmAccessPoint          *access_point);
guint            nm_access_point_get_strength                   (NmAccessPoint          *access_point);
const gchar     *nm_access_point_get_ssid                       (NmAccessPoint          *access_point);
const gchar     *nm_access_point_get_object_path                (NmAccessPoint          *access_point);
const gchar     *nm_access_point_mode_to_localized_string       (NmAccessPointMode       mode);

G_END_DECLS

#endif /* __NM_ACCESS_POINT_H */

