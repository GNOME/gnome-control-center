/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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

#ifndef __NET_OBJECT_H
#define __NET_OBJECT_H

#include <glib-object.h>
#include <gtk/gtk.h>
#include <NetworkManager.h>

#include "cc-network-panel.h"

G_BEGIN_DECLS

#define NET_TYPE_OBJECT          (net_object_get_type ())
#define NET_OBJECT(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), NET_TYPE_OBJECT, NetObject))
#define NET_OBJECT_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), NET_TYPE_OBJECT, NetObjectClass))
#define NET_IS_OBJECT(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), NET_TYPE_OBJECT))
#define NET_IS_OBJECT_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), NET_TYPE_OBJECT))
#define NET_OBJECT_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), NET_TYPE_OBJECT, NetObjectClass))

typedef struct _NetObjectPrivate         NetObjectPrivate;
typedef struct _NetObject                NetObject;
typedef struct _NetObjectClass           NetObjectClass;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (NetObject, g_object_unref)

struct _NetObject
{
         GObject                 parent;
         NetObjectPrivate       *priv;
};

struct _NetObjectClass
{
        GObjectClass             parent_class;

        /* vtable */
        GtkWidget               *(*add_to_notebook)    (NetObject       *object,
                                                        GtkNotebook     *notebook,
                                                        GtkSizeGroup    *heading_size_group);
        void                     (*delete)              (NetObject       *object);
        void                     (*refresh)             (NetObject       *object);
        void                     (*edit)                (NetObject       *object);

        /* signal */
        void                     (* changed)            (NetObject      *object);
        void                     (* removed)            (NetObject      *object);
};

GType            net_object_get_type                    (void);
const gchar     *net_object_get_id                      (NetObject      *object);
void             net_object_set_id                      (NetObject      *object,
                                                         const gchar    *id);
const gchar     *net_object_get_title                   (NetObject      *object);
void             net_object_set_title                   (NetObject      *object,
                                                         const gchar    *title);
NMClient        *net_object_get_client                  (NetObject      *object);
GCancellable    *net_object_get_cancellable             (NetObject      *object);
CcNetworkPanel  *net_object_get_panel                   (NetObject      *object);
void             net_object_emit_changed                (NetObject      *object);
void             net_object_emit_removed                (NetObject      *object);
void             net_object_delete                      (NetObject      *object);
void             net_object_refresh                     (NetObject      *object);
void             net_object_edit                        (NetObject      *object);
GtkWidget       *net_object_add_to_notebook             (NetObject      *object,
                                                         GtkNotebook    *notebook,
                                                         GtkSizeGroup   *heading_size_group);
gboolean         net_object_get_removable               (NetObject      *object);
void             net_object_set_removable               (NetObject      *object,
                                                         gboolean        removable);

G_END_DECLS

#endif /* __NET_OBJECT_H */

