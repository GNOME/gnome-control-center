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

#ifndef __GSD_KERBEROS_IDENTITY_H__
#define __GSD_KERBEROS_IDENTITY_H__

#include <glib.h>
#include <glib-object.h>

#include <krb5.h>
#include "gsd-kerberos-identity-inquiry.h"

G_BEGIN_DECLS

#define GSD_TYPE_KERBEROS_IDENTITY             (gsd_kerberos_identity_get_type ())
#define GSD_KERBEROS_IDENTITY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSD_TYPE_KERBEROS_IDENTITY, GsdKerberosIdentity))
#define GSD_KERBEROS_IDENTITY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GSD_TYPE_KERBEROS_IDENTITY, GsdKerberosIdentityClass))
#define GSD_IS_KERBEROS_IDENTITY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSD_TYPE_KERBEROS_IDENTITY))
#define GSD_IS_KERBEROS_IDENTITY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GSD_TYPE_KERBEROS_IDENTITY))
#define GSD_KERBEROS_IDENTITY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GSD_TYPE_KERBEROS_IDENTITY, GsdKerberosIdentityClass))

typedef struct _GsdKerberosIdentity        GsdKerberosIdentity;
typedef struct _GsdKerberosIdentityClass   GsdKerberosIdentityClass;
typedef struct _GsdKerberosIdentityPrivate GsdKerberosIdentityPrivate;
typedef enum _GsdKerberosIdentityDescriptionLevel GsdKerberosIdentityDescriptionLevel;

enum _GsdKerberosIdentityDescriptionLevel
{
        GSD_KERBEROS_IDENTITY_DESCRIPTION_REALM,
        GSD_KERBEROS_IDENTITY_DESCRIPTION_USERNAME_AND_REALM,
        GSD_KERBEROS_IDENTITY_DESCRIPTION_USERNAME_ROLE_AND_REALM
};

struct _GsdKerberosIdentity
{
        GObject            parent;

        GsdKerberosIdentityPrivate *priv;
};

struct _GsdKerberosIdentityClass
{
        GObjectClass parent_class;
};

GType         gsd_kerberos_identity_get_type (void);

GsdIdentity  *gsd_kerberos_identity_new      (krb5_context  kerberos_context,
                                              krb5_ccache   cache);

gboolean      gsd_kerberos_identity_sign_in  (GsdKerberosIdentity              *self,
                                              const char                       *principal_name,
                                              GsdIdentityInquiryFunc            inquiry_func,
                                              gpointer                          inquiry_data,
                                              GDestroyNotify                    destroy_notify,
                                              GCancellable                     *cancellable,
                                              GError                          **error);
void          gsd_kerberos_identity_update (GsdKerberosIdentity *identity,
                                            GsdKerberosIdentity *new_identity);
gboolean      gsd_kerberos_identity_renew  (GsdKerberosIdentity *self,
                                            GError             **error);
gboolean      gsd_kerberos_identity_erase  (GsdKerberosIdentity *self,
                                            GError             **error);

char         *gsd_kerberos_identity_get_principal_name (GsdKerberosIdentity *self);
char         *gsd_kerberos_identity_get_realm_name     (GsdKerberosIdentity *self);
G_END_DECLS

#endif /* __GSD_KERBEROS_IDENTITY_H__ */
