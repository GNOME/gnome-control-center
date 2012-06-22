/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Authors: Ray Strode <rstrode@redhat.com>
 */

#ifndef __GSD_IDENTITY_H__
#define __GSD_IDENTITY_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GSD_TYPE_IDENTITY             (gsd_identity_get_type ())
#define GSD_IDENTITY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSD_TYPE_IDENTITY, GsdIdentity))
#define GSD_IDENTITY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GSD_TYPE_IDENTITY, GsdIdentityInterface))
#define GSD_IS_IDENTITY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSD_TYPE_IDENTITY))
#define GSD_IDENTITY_GET_IFACE(obj)   (G_TYPE_INSTANCE_GET_INTERFACE((obj), GSD_TYPE_IDENTITY, GsdIdentityInterface))
#define GSD_IDENTITY_ERROR            (gsd_identity_error_quark ())

typedef struct _GsdIdentity          GsdIdentity;
typedef struct _GsdIdentityInterface GsdIdentityInterface;
typedef enum   _GsdIdentityError     GsdIdentityError;

struct _GsdIdentityInterface
{
        GTypeInterface base_interface;

        const char * (* get_identifier)  (GsdIdentity *identity);
        gboolean     (* is_signed_in)    (GsdIdentity *identity);
};

enum _GsdIdentityError
{
        GSD_IDENTITY_ERROR_VERIFYING,
        GSD_IDENTITY_ERROR_SIGNING_IN,
        GSD_IDENTITY_ERROR_RENEWING,
        GSD_IDENTITY_ERROR_ERASING
};

GType       gsd_identity_get_type         (void);
GQuark      gsd_identity_error_quark      (void);

const char *gsd_identity_get_identifier   (GsdIdentity *identity);
gboolean    gsd_identity_is_signed_in     (GsdIdentity *identity);

G_END_DECLS

#endif /* __GSD_IDENTITY_H__ */
