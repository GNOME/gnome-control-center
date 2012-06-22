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
 * Authors: Ray Strode
 */

#ifndef __GSD_KERBEROS_IDENTITY_MANAGER_H__
#define __GSD_KERBEROS_IDENTITY_MANAGER_H__

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "gsd-identity-manager.h"

G_BEGIN_DECLS

#define GSD_TYPE_KERBEROS_IDENTITY_MANAGER           (gsd_kerberos_identity_manager_get_type ())
#define GSD_KERBEROS_IDENTITY_MANAGER(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GSD_TYPE_KERBEROS_IDENTITY_MANAGER, GsdKerberosIdentityManager))
#define GSD_KERBEROS_IDENTITY_MANAGER_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GSD_TYPE_KERBEROS_IDENTITY_MANAGER, GsdKerberosIdentityManagerClass))
#define GSD_IS_KERBEROS_IDENTITY_MANAGER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GSD_TYPE_KERBEROS_IDENTITY_MANAGER))
#define GSD_IS_KERBEROS_IDENTITY_MANAGER_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GSD_TYPE_KERBEROS_IDENTITY_MANAGER))
#define GSD_KERBEROS_IDENTITY_MANAGER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GSD_TYPE_KERBEROS_IDENTITY_MANAGER, GsdKerberosIdentityManagerClass))

typedef struct _GsdKerberosIdentityManager           GsdKerberosIdentityManager;
typedef struct _GsdKerberosIdentityManagerClass      GsdKerberosIdentityManagerClass;
typedef struct _GsdKerberosIdentityManagerPrivate    GsdKerberosIdentityManagerPrivate; struct _GsdKerberosIdentityManager
{
        GObject parent_instance;
        GsdKerberosIdentityManagerPrivate *priv;
};

struct _GsdKerberosIdentityManagerClass
{
        GObjectClass parent_class;
};

GType                   gsd_kerberos_identity_manager_get_type  (void);
GsdIdentityManager*     gsd_kerberos_identity_manager_new       (void);

void                    gsd_kerberos_identity_manager_start_test (GsdKerberosIdentityManager  *manager,
                                                                  GError                     **error);
G_END_DECLS

#endif /* __GSD_KERBEROS_IDENTITY_MANAGER_H__ */
