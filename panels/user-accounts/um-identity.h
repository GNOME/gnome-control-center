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

#ifndef __UM_IDENTITY_H__
#define __UM_IDENTITY_H__

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define UM_TYPE_IDENTITY             (um_identity_get_type ())
#define UM_IDENTITY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), UM_TYPE_IDENTITY, UmIdentity))
#define UM_IDENTITY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), UM_TYPE_IDENTITY, UmIdentityInterface))
#define UM_IS_IDENTITY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), UM_TYPE_IDENTITY))
#define UM_IDENTITY_GET_IFACE(obj)   (G_TYPE_INSTANCE_GET_INTERFACE((obj), UM_TYPE_IDENTITY, UmIdentityInterface))
#define UM_IDENTITY_ERROR            (um_identity_error_quark ())

typedef struct _UmIdentity          UmIdentity;
typedef struct _UmIdentityInterface UmIdentityInterface;
typedef enum   _UmIdentityError     UmIdentityError;

struct _UmIdentityInterface
{
        GTypeInterface base_interface;

        const char * (* get_identifier)  (UmIdentity *identity);
        gboolean     (* is_signed_in)    (UmIdentity *identity);
};

enum _UmIdentityError {
        UM_IDENTITY_ERROR_VERIFYING,
        UM_IDENTITY_ERROR_ERASING
};

GType       um_identity_get_type         (void);
GQuark      um_identity_error_quark      (void);

const char *um_identity_get_identifier   (UmIdentity *identity);
gboolean    um_identity_is_signed_in     (UmIdentity *identity);

G_END_DECLS

#endif /* __UM_IDENTITY_H__ */
