/* -*- Mode: C; tab-width: 8; ident-tabs-mode: nil; c-basic-offset: 8 -*-
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
 *
 * Author: Ray Strode
 */

#include "config.h"

#include "gsd-identity-inquiry.h"
#include "gsd-identity-inquiry-private.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

enum {
        COMPLETE,
        NUMBER_OF_SIGNALS,
};

static guint signals[NUMBER_OF_SIGNALS] = { 0 };

G_DEFINE_INTERFACE (GsdIdentityInquiry,
                    gsd_identity_inquiry,
                    G_TYPE_OBJECT);

static void
gsd_identity_inquiry_default_init (GsdIdentityInquiryInterface *interface)
{
        signals[COMPLETE] = g_signal_new ("complete",
                                          G_TYPE_FROM_INTERFACE (interface),
                                          G_SIGNAL_RUN_LAST,
                                          0,
                                          NULL, NULL, NULL,
                                          G_TYPE_NONE, 0);
}

void
_gsd_identity_inquiry_emit_complete (GsdIdentityInquiry *self)
{
        g_signal_emit (G_OBJECT (self), signals[COMPLETE], 0);
}

char *
gsd_identity_inquiry_get_name (GsdIdentityInquiry *self)
{
        g_return_val_if_fail (GSD_IS_IDENTITY_INQUIRY (self), NULL);

        return GSD_IDENTITY_INQUIRY_GET_IFACE (self)->get_name (self);
}

char *
gsd_identity_inquiry_get_banner (GsdIdentityInquiry *self)
{
        g_return_val_if_fail (GSD_IS_IDENTITY_INQUIRY (self), NULL);

        return GSD_IDENTITY_INQUIRY_GET_IFACE (self)->get_banner (self);
}

gboolean
gsd_identity_inquiry_is_complete (GsdIdentityInquiry *self)
{
        g_return_val_if_fail (GSD_IS_IDENTITY_INQUIRY (self), TRUE);

        return GSD_IDENTITY_INQUIRY_GET_IFACE (self)->is_complete (self);
}

void
gsd_identity_inquiry_iter_init (GsdIdentityInquiryIter *iter,
                                GsdIdentityInquiry     *inquiry)
{
        g_return_if_fail (GSD_IS_IDENTITY_INQUIRY (inquiry));

        GSD_IDENTITY_INQUIRY_GET_IFACE (inquiry)->iter_init (iter, inquiry);
}

GsdIdentityQuery *
gsd_identity_inquiry_iter_next (GsdIdentityInquiryIter *iter,
                                GsdIdentityInquiry     *inquiry)
{
        g_return_val_if_fail (GSD_IS_IDENTITY_INQUIRY (inquiry), NULL);

        return GSD_IDENTITY_INQUIRY_GET_IFACE (inquiry)->iter_next (iter, inquiry);
}

GsdIdentity *
gsd_identity_inquiry_get_identity (GsdIdentityInquiry *self)
{
        g_return_val_if_fail (GSD_IS_IDENTITY_INQUIRY (self), NULL);

        return GSD_IDENTITY_INQUIRY_GET_IFACE (self)->get_identity (self);
}

GsdIdentityQueryMode
gsd_identity_query_get_mode (GsdIdentityInquiry *self,
                             GsdIdentityQuery   *query)
{
        g_return_val_if_fail (GSD_IS_IDENTITY_INQUIRY (self),
                              GSD_IDENTITY_QUERY_MODE_INVISIBLE);

        return GSD_IDENTITY_INQUIRY_GET_IFACE (self)->get_mode (self, query);
}

char *
gsd_identity_query_get_prompt (GsdIdentityInquiry *self,
                               GsdIdentityQuery   *query)
{
        g_return_val_if_fail (GSD_IS_IDENTITY_INQUIRY (self), NULL);

        return GSD_IDENTITY_INQUIRY_GET_IFACE (self)->get_prompt (self, query);
}

void
gsd_identity_inquiry_answer_query (GsdIdentityInquiry *self,
                                   GsdIdentityQuery   *query,
                                   const char         *answer)
{
        g_return_if_fail (GSD_IS_IDENTITY_INQUIRY (self));

        GSD_IDENTITY_INQUIRY_GET_IFACE (self)->answer_query (self, query, answer);
}

gboolean
gsd_identity_query_is_answered (GsdIdentityInquiry *self,
                                GsdIdentityQuery   *query)
{
        g_return_val_if_fail (GSD_IS_IDENTITY_INQUIRY (self), FALSE);

        return GSD_IDENTITY_INQUIRY_GET_IFACE (self)->is_answered (self, query);
}
