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

G_DEFINE_TYPE (NetObject, net_object, G_TYPE_OBJECT)

GtkWidget *
net_object_get_widget (NetObject    *self,
                       GtkSizeGroup *heading_size_group)
{
        NetObjectClass *klass = NET_OBJECT_GET_CLASS (self);

        return klass->get_widget (self, heading_size_group);
}

static void
net_object_class_init (NetObjectClass *klass)
{
}

static void
net_object_init (NetObject *self)
{
}

