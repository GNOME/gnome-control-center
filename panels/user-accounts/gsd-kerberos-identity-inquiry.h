/* -*- Mode: C; tab-width: 8; ident-tabs-mode: nil; c-basic-offset: 8 -*-
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

#ifndef __GSD_KERBEROS_IDENTITY_INQUIRY_H__
#define __GSD_KERBEROS_IDENTITY_INQUIRY_H__

#include <stdint.h>

#include <glib.h>
#include <glib-object.h>

#include "gsd-identity-inquiry.h"
#include "gsd-kerberos-identity.h"

G_BEGIN_DECLS

#define GSD_TYPE_KERBEROS_IDENTITY_INQUIRY             (gsd_kerberos_identity_inquiry_get_type ())
#define GSD_KERBEROS_IDENTITY_INQUIRY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSD_TYPE_KERBEROS_IDENTITY_INQUIRY, GsdKerberosIdentityInquiry))
#define GSD_KERBEROS_IDENTITY_INQUIRY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GSD_TYPE_KERBEROS_IDENTITY_INQUIRY, GsdKerberosIdentityInquiryClass))
#define GSD_IS_KERBEROS_IDENTITY_INQUIRY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSD_TYPE_KERBEROS_IDENTITY_INQUIRY))
#define GSD_IS_KERBEROS_IDENTITY_INQUIRY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GSD_TYPE_KERBEROS_IDENTITY_INQUIRY))
#define GSD_KERBEROS_IDENTITY_INQUIRY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GSD_TYPE_KERBEROS_IDENTITY_INQUIRY, GsdKerberosIdentityInquiryClass))
typedef struct _GsdKerberosIdentity               GsdKerberosIdentity;
typedef struct _GsdKerberosIdentityInquiry        GsdKerberosIdentityInquiry;
typedef struct _GsdKerberosIdentityInquiryClass   GsdKerberosIdentityInquiryClass;
typedef struct _GsdKerberosIdentityInquiryPrivate GsdKerberosIdentityInquiryPrivate;
typedef struct _GsdKerberosIdentityInquiryIter    GsdKerberosIdentityInquiryIter;

typedef enum
{
        GSD_KERBEROS_IDENTITY_QUERY_MODE_INVISIBLE,
        GSD_KERBEROS_IDENTITY_QUERY_MODE_VISIBLE
} GsdKerberosIdentityQueryMode;

struct _GsdKerberosIdentityInquiry
{
        GObject            parent;

        GsdKerberosIdentityInquiryPrivate *priv;
};

struct _GsdKerberosIdentityInquiryClass
{
        GObjectClass parent_class;
};

GType         gsd_kerberos_identity_inquiry_get_type (void);

GsdIdentityInquiry  *gsd_kerberos_identity_inquiry_new      (GsdKerberosIdentity *identity,
                                                             const char          *name,
                                                             const char          *banner,
                                                             krb5_prompt          prompts[],
                                                             int                  number_of_prompts);

#endif /* __GSD_KERBEROS_IDENTITY_INQUIRY_H__ */
