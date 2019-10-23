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
#include <arpa/inet.h>
#include <netinet/ether.h>

#include <NetworkManager.h>

#include "net-device.h"

typedef struct
{
        NMDevice                        *nm_device;
} NetDevicePrivate;

enum {
        PROP_0,
        PROP_DEVICE,
        PROP_LAST
};

G_DEFINE_TYPE_WITH_PRIVATE (NetDevice, net_device, NET_TYPE_OBJECT)

NMDevice *
net_device_get_nm_device (NetDevice *self)
{
        NetDevicePrivate *priv;

        g_return_val_if_fail (NET_IS_DEVICE (self), NULL);

        priv = net_device_get_instance_private (self);
        return priv->nm_device;
}

/**
 * net_device_get_property:
 **/
static void
net_device_get_property (GObject *object,
                         guint prop_id,
                         GValue *value,
                         GParamSpec *pspec)
{
        NetDevice *net_device = NET_DEVICE (object);
        NetDevicePrivate *priv = net_device_get_instance_private (net_device);

        switch (prop_id) {
        case PROP_DEVICE:
                g_value_set_object (value, priv->nm_device);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

/**
 * net_device_set_property:
 **/
static void
net_device_set_property (GObject *object,
                         guint prop_id,
                         const GValue *value,
                         GParamSpec *pspec)
{
        NetDevice *net_device = NET_DEVICE (object);
        NetDevicePrivate *priv = net_device_get_instance_private (net_device);

        switch (prop_id) {
        case PROP_DEVICE:
                priv->nm_device = g_value_dup_object (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
net_device_finalize (GObject *object)
{
        NetDevice *self = NET_DEVICE (object);
        NetDevicePrivate *priv = net_device_get_instance_private (self);

        g_clear_object (&priv->nm_device);

        G_OBJECT_CLASS (net_device_parent_class)->finalize (object);
}

static void
net_device_class_init (NetDeviceClass *klass)
{
        GParamSpec *pspec;
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = net_device_finalize;
        object_class->get_property = net_device_get_property;
        object_class->set_property = net_device_set_property;

        pspec = g_param_spec_object ("nm-device", NULL, NULL,
                                     NM_TYPE_DEVICE,
                                     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
        g_object_class_install_property (object_class, PROP_DEVICE, pspec);
}

static void
net_device_init (NetDevice *self)
{
}
