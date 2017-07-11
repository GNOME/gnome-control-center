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
        gboolean                         removable;
        GCancellable                    *cancellable;
        NMClient                        *client;
        CcNetworkPanel                  *panel;
};

enum {
        PROP_0,
        PROP_ID,
        PROP_TITLE,
        PROP_REMOVABLE,
        PROP_CLIENT,
        PROP_CANCELLABLE,
        PROP_PANEL,
        PROP_LAST
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
        g_clear_pointer (&object->priv->id, g_free);
        object->priv->id = g_strdup (id);
        g_object_notify (G_OBJECT (object), "id");
}

gboolean
net_object_get_removable (NetObject *object)
{
        g_return_val_if_fail (NET_IS_OBJECT (object), FALSE);
        return object->priv->removable;
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
        g_clear_pointer (&object->priv->title, g_free);
        object->priv->title = g_strdup (title);
        g_object_notify (G_OBJECT (object), "title");
}

NMClient *
net_object_get_client (NetObject *object)
{
        g_return_val_if_fail (NET_IS_OBJECT (object), NULL);
        return object->priv->client;
}

GCancellable *
net_object_get_cancellable (NetObject *object)
{
        g_return_val_if_fail (NET_IS_OBJECT (object), NULL);
        return object->priv->cancellable;
}

CcNetworkPanel *
net_object_get_panel (NetObject *object)
{
        g_return_val_if_fail (NET_IS_OBJECT (object), NULL);
        return object->priv->panel;
}

GtkWidget *
net_object_add_to_stack (NetObject    *object,
                         GtkStack     *stack,
                         GtkSizeGroup *heading_size_group)
{
        GtkWidget *widget;
        NetObjectClass *klass = NET_OBJECT_GET_CLASS (object);
        if (klass->add_to_stack != NULL) {
                widget = klass->add_to_stack (object, stack, heading_size_group);
                g_object_set_data_full (G_OBJECT (widget),
                                        "NetObject::id",
                                        g_strdup (object->priv->id),
                                        g_free);
                return widget;
        }
        g_debug ("no klass->add_to_stack for %s", object->priv->id);
        return NULL;
}

void
net_object_delete (NetObject *object)
{
        NetObjectClass *klass = NET_OBJECT_GET_CLASS (object);
        if (klass->delete != NULL)
                klass->delete (object);
}

void
net_object_refresh (NetObject *object)
{
        NetObjectClass *klass = NET_OBJECT_GET_CLASS (object);
        if (klass->refresh != NULL)
                klass->refresh (object);
}

void
net_object_edit (NetObject *object)
{
        NetObjectClass *klass = NET_OBJECT_GET_CLASS (object);
        if (klass->edit != NULL)
                klass->edit (object);
}

/**
 * net_object_get_property:
 **/
static void
net_object_get_property (GObject *object_,
                         guint prop_id,
                         GValue *value,
                         GParamSpec *pspec)
{
        NetObject *object = NET_OBJECT (object_);
        NetObjectPrivate *priv = object->priv;

        switch (prop_id) {
        case PROP_ID:
                g_value_set_string (value, priv->id);
                break;
        case PROP_TITLE:
                g_value_set_string (value, priv->title);
                break;
        case PROP_REMOVABLE:
                g_value_set_boolean (value, priv->removable);
                break;
        case PROP_CLIENT:
                g_value_set_pointer (value, priv->client);
                break;
        case PROP_CANCELLABLE:
                g_value_set_object (value, priv->cancellable);
                break;
        case PROP_PANEL:
                g_value_set_pointer (value, priv->panel);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

/**
 * net_object_set_property:
 **/
static void
net_object_set_property (GObject *object_,
                         guint prop_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
        NetObject *object = NET_OBJECT (object_);
        NetObjectPrivate *priv = object->priv;

        switch (prop_id) {
        case PROP_ID:
                g_free (priv->id);
                priv->id = g_strdup (g_value_get_string (value));
                break;
        case PROP_TITLE:
                g_free (priv->title);
                priv->title = g_strdup (g_value_get_string (value));
                break;
        case PROP_REMOVABLE:
                priv->removable = g_value_get_boolean (value);
                break;
        case PROP_CLIENT:
                priv->client = g_value_get_pointer (value);
                if (priv->client)
                        g_object_add_weak_pointer (G_OBJECT (priv->client), (gpointer *) (&priv->client));
                break;
        case PROP_CANCELLABLE:
                g_assert (!priv->cancellable);
                priv->cancellable = g_value_dup_object (value);
                break;
        case PROP_PANEL:
                g_assert (!priv->panel);
                priv->panel = g_value_get_pointer (value);
                if (priv->panel)
                        g_object_add_weak_pointer (G_OBJECT (priv->panel), (gpointer *) (&priv->panel));
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
net_object_finalize (GObject *object)
{
        NetObject *nm_object = NET_OBJECT (object);
        NetObjectPrivate *priv = nm_object->priv;

        g_free (priv->id);
        g_free (priv->title);
        if (priv->cancellable != NULL)
                g_object_unref (priv->cancellable);

        if (priv->client)
                g_object_remove_weak_pointer (G_OBJECT (priv->client), (gpointer *) (&priv->client));
        if (priv->panel)
                g_object_remove_weak_pointer (G_OBJECT (priv->panel), (gpointer *) (&priv->panel));

        G_OBJECT_CLASS (net_object_parent_class)->finalize (object);
}

static void
net_object_class_init (NetObjectClass *klass)
{
        GParamSpec *pspec;
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        object_class->finalize = net_object_finalize;
        object_class->get_property = net_object_get_property;
        object_class->set_property = net_object_set_property;

        pspec = g_param_spec_string ("id", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE);
        g_object_class_install_property (object_class, PROP_ID, pspec);

        pspec = g_param_spec_string ("title", NULL, NULL,
                                     NULL,
                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
        g_object_class_install_property (object_class, PROP_TITLE, pspec);

        pspec = g_param_spec_boolean ("removable", NULL, NULL,
                                      TRUE,
                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
        g_object_class_install_property (object_class, PROP_REMOVABLE, pspec);

        pspec = g_param_spec_pointer ("client", NULL, NULL,
                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
        g_object_class_install_property (object_class, PROP_CLIENT, pspec);

        pspec = g_param_spec_object ("cancellable", NULL, NULL,
                                     G_TYPE_CANCELLABLE,
                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
        g_object_class_install_property (object_class, PROP_CANCELLABLE, pspec);

        pspec = g_param_spec_pointer ("panel", NULL, NULL,
                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
        g_object_class_install_property (object_class, PROP_PANEL, pspec);

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

