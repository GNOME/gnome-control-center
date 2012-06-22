/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "config.h"

#include <glib-object.h>
#include <glib/gi18n.h>

#include "gsd-identity.h"

G_DEFINE_INTERFACE (GsdIdentity, gsd_identity, G_TYPE_OBJECT);

static void
gsd_identity_default_init (GsdIdentityInterface *interface)
{
}

GQuark
gsd_identity_error_quark (void)
{
        static GQuark error_quark = 0;

        if (error_quark == 0) {
                error_quark = g_quark_from_static_string ("gsd-identity-error");
        }

        return error_quark;
}

const char *
gsd_identity_get_identifier (GsdIdentity *self)
{
        return GSD_IDENTITY_GET_IFACE (self)->get_identifier (self);
}

gboolean
gsd_identity_is_signed_in (GsdIdentity *self)
{
        return GSD_IDENTITY_GET_IFACE (self)->is_signed_in (self);
}
