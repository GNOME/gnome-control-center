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

#ifndef __NM_DEVICE_H
#define __NM_DEVICE_H

#include <glib-object.h>

G_BEGIN_DECLS

#define NM_TYPE_DEVICE          (nm_device_get_type ())
#define NM_DEVICE(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), NM_TYPE_DEVICE, NmDevice))
#define NM_DEVICE_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), NM_TYPE_DEVICE, NmDeviceClass))
#define NM_IS_DEVICE(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), NM_TYPE_DEVICE))
#define NM_IS_DEVICE_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), NM_TYPE_DEVICE))
#define NM_DEVICE_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), NM_TYPE_DEVICE, NmDeviceClass))

typedef struct _NmDevicePrivate         NmDevicePrivate;
typedef struct _NmDevice                NmDevice;
typedef struct _NmDeviceClass           NmDeviceClass;

struct _NmDevice
{
         GObject                 parent;
         NmDevicePrivate        *priv;
};

struct _NmDeviceClass
{
        GObjectClass                 parent_class;
        void                        (* ready)                   (NmDevice        *device);
        void                        (* changed)                 (NmDevice        *device);
};

typedef enum {
        NM_DEVICE_KIND_UNKNOWN,
        NM_DEVICE_KIND_ETHERNET,
        NM_DEVICE_KIND_WIFI,
        NM_DEVICE_KIND_GSM,
        NM_DEVICE_KIND_CDMA,
        NM_DEVICE_KIND_BLUETOOTH,
        NM_DEVICE_KIND_MESH
} NmDeviceKind;

typedef enum {
        NM_DEVICE_STATE_UNKNOWN,
        NM_DEVICE_STATE_UNMANAGED,
        NM_DEVICE_STATE_UNAVAILABLE,
        NM_DEVICE_STATE_DISCONNECTED,
        NM_DEVICE_STATE_PREPARE,
        NM_DEVICE_STATE_CONFIG,
        NM_DEVICE_STATE_NEED_AUTH,
        NM_DEVICE_STATE_IP_CONFIG,
        NM_DEVICE_STATE_ACTIVATED,
        NM_DEVICE_STATE_FAILED
} NmDeviceState;

GType            nm_device_get_type                     (void);
NmDevice        *nm_device_new                          (void);

void             nm_device_refresh                      (NmDevice       *device,
                                                         const gchar    *object_path,
                                                         GCancellable   *cancellable);
const gchar     *nm_device_get_active_access_point      (NmDevice       *device);
const gchar     *nm_device_get_ip4_address              (NmDevice       *device);
const gchar     *nm_device_get_ip4_nameserver           (NmDevice       *device);
const gchar     *nm_device_get_ip4_route                (NmDevice       *device);
const gchar     *nm_device_get_ip4_subnet_mask          (NmDevice       *device);
const gchar     *nm_device_get_ip6_address              (NmDevice       *device);
const gchar     *nm_device_get_ip6_nameserver           (NmDevice       *device);
const gchar     *nm_device_get_ip6_route                (NmDevice       *device);
const gchar     *nm_device_get_mac_address              (NmDevice       *device);
const gchar     *nm_device_get_modem_imei               (NmDevice       *device);
const gchar     *nm_device_get_object_path              (NmDevice       *device);
const gchar     *nm_device_get_operator_name            (NmDevice       *device);
const gchar     *nm_device_get_speed                    (NmDevice       *device);
NmDeviceState    nm_device_get_state                    (NmDevice       *device);
NmDeviceKind     nm_device_get_kind                     (NmDevice       *device);
GPtrArray       *nm_device_get_access_points            (NmDevice       *device);

const gchar     *nm_device_kind_to_icon_name            (NmDeviceKind    kind);
const gchar     *nm_device_kind_to_localized_string     (NmDeviceKind    kind);
const gchar     *nm_device_kind_to_sortable_string      (NmDeviceKind    kind);
const gchar     *nm_device_state_to_localized_string    (NmDeviceState   kind);

G_END_DECLS

#endif /* __NM_DEVICE_H */

