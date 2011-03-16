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

#include "config.h"

#include <glib-object.h>
#include <glib/gi18n.h>

#include "net-object.h"

#define NET_OBJECT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NET_TYPE_OBJECT, NetObjectPrivate))

struct _NetObjectPrivate
{
        gchar                           *id;
        gchar                           *title;
};

enum {
        SIGNAL_CHANGED,
        SIGNAL_REMOVED,
        SIGNAL_LAST
};

static guint signals[SIGNAL_LAST] = { 0 };
G_DEFINE_TYPE (NetObject, net_object, G_TYPE_OBJECT)

void
net_object_emit_changed (NetObject *object)
{
        g_return_if_fail (NET_IS_OBJECT (object));
        g_debug ("NetObject: %s emit 'changed'", object->priv->id);
        g_signal_emit (object, signals[SIGNAL_CHANGED], 0);
}

void
net_object_emit_removed (NetObject *object)
{
        g_return_if_fail (NET_IS_OBJECT (object));
        g_debug ("NetObject: %s emit 'removed'", object->priv->id);
        g_signal_emit (object, signals[SIGNAL_REMOVED], 0);
}

const gchar *
net_object_get_id (NetObject *object)
{
        g_return_val_if_fail (NET_IS_OBJECT (object), NULL);
        return object->priv->id;
}

void
net_object_set_id (NetObject *object, const gchar *id)
{
        g_return_if_fail (NET_IS_OBJECT (object));
        object->priv->id = g_strdup (id);
}

const gchar *
net_object_get_title (NetObject *object)
{
        g_return_val_if_fail (NET_IS_OBJECT (object), NULL);
        return object->priv->title;
}

void
net_object_set_title (NetObject *object, const gchar *title)
{
        g_return_if_fail (NET_IS_OBJECT (object));
        object->priv->title = g_strdup (title);
}

static void
net_object_finalize (GObject *object)
{
        NetObject *nm_object = NET_OBJECT (object);
        NetObjectPrivate *priv = nm_object->priv;

        g_free (priv->id);
        g_free (priv->title);

        G_OBJECT_CLASS (net_object_parent_class)->finalize (object);
}

static void
net_object_class_init (NetObjectClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = net_object_finalize;

        signals[SIGNAL_CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (NetObjectClass, changed),
                              NULL, NULL, g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);
        signals[SIGNAL_REMOVED] =
                g_signal_new ("removed",
                              G_TYPE_FROM_CLASS (object_class), G_SIGNAL_RUN_LAST,
                              G_STRUCT_OFFSET (NetObjectClass, changed),
                              NULL, NULL, g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        g_type_class_add_private (klass, sizeof (NetObjectPrivate));
}

static void
net_object_init (NetObject *object)
{
        object->priv = NET_OBJECT_GET_PRIVATE (object);
}

NetObject *
net_object_new (void)
{
        NetObject *object;
        object = g_object_new (NET_TYPE_OBJECT, NULL);
        return NET_OBJECT (object);
}

