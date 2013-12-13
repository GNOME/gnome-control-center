/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Red Hat, Inc.
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

#ifndef __NET_DEVICE_TEAM_H
#define __NET_DEVICE_TEAM_H

#include <glib-object.h>

#include "net-virtual-device.h"

G_BEGIN_DECLS

#define NET_TYPE_DEVICE_TEAM          (net_device_team_get_type ())
#define NET_DEVICE_TEAM(o)            (G_TYPE_CHECK_INSTANCE_CAST ((o), NET_TYPE_DEVICE_TEAM, NetDeviceTeam))
#define NET_DEVICE_TEAM_CLASS(k)      (G_TYPE_CHECK_CLASS_CAST((k), NET_TYPE_DEVICE_TEAM, NetDeviceTeamClass))
#define NET_IS_DEVICE_TEAM(o)         (G_TYPE_CHECK_INSTANCE_TYPE ((o), NET_TYPE_DEVICE_TEAM))
#define NET_IS_DEVICE_TEAM_CLASS(k)   (G_TYPE_CHECK_CLASS_TYPE ((k), NET_TYPE_DEVICE_TEAM))
#define NET_DEVICE_TEAM_GET_CLASS(o)  (G_TYPE_INSTANCE_GET_CLASS ((o), NET_TYPE_DEVICE_TEAM, NetDeviceTeamClass))

typedef struct _NetDeviceTeamPrivate   NetDeviceTeamPrivate;
typedef struct _NetDeviceTeam          NetDeviceTeam;
typedef struct _NetDeviceTeamClass     NetDeviceTeamClass;

struct _NetDeviceTeam
{
         NetVirtualDevice               parent;
         NetDeviceTeamPrivate          *priv;
};

struct _NetDeviceTeamClass
{
        NetVirtualDeviceClass            parent_class;
};

GType            net_device_team_get_type      (void);

G_END_DECLS

#endif /* __NET_DEVICE_TEAM_H */

