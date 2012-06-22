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

#include "gsd-kerberos-identity-inquiry.h"
#include "gsd-identity-inquiry-private.h"

#include <string.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

struct _GsdKerberosIdentityInquiryPrivate
{
        GsdIdentity *identity;
        char        *name;
        char        *banner;
        GList       *queries;
        int          number_of_queries;
        int          number_of_unanswered_queries;
};

typedef struct
{
        GsdIdentityInquiry *inquiry;
        krb5_prompt        *kerberos_prompt;
        gboolean            is_answered;
} GsdKerberosIdentityQuery;

static void identity_inquiry_interface_init (GsdIdentityInquiryInterface *interface);
static void initable_interface_init (GInitableIface *interface);

G_DEFINE_TYPE_WITH_CODE (GsdKerberosIdentityInquiry,
                         gsd_kerberos_identity_inquiry,
                         G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                initable_interface_init)
                         G_IMPLEMENT_INTERFACE (GSD_TYPE_IDENTITY_INQUIRY,
                                                identity_inquiry_interface_init));

static gboolean
gsd_kerberos_identity_inquiry_initable_init (GInitable      *initable,
                                             GCancellable   *cancellable,
                                             GError        **error)
{
        if (g_cancellable_set_error_if_cancelled (cancellable, error)) {
                return FALSE;
        }

        return TRUE;
}

static void
initable_interface_init (GInitableIface *interface)
{
        interface->init = gsd_kerberos_identity_inquiry_initable_init;
}

static GsdKerberosIdentityQuery *
gsd_kerberos_identity_query_new (GsdIdentityInquiry *inquiry,
                                 krb5_prompt        *kerberos_prompt)
{
        GsdKerberosIdentityQuery *query;

        query = g_slice_new (GsdKerberosIdentityQuery);
        query->inquiry = inquiry;
        query->kerberos_prompt = kerberos_prompt;
        query->is_answered = FALSE;

        return query;
}

static void
gsd_kerberos_identity_query_free (GsdKerberosIdentityQuery *query)
{
        g_slice_free (GsdKerberosIdentityQuery, query);
}

static void
gsd_kerberos_identity_inquiry_dispose (GObject *object)
{
        GsdKerberosIdentityInquiry *self = GSD_KERBEROS_IDENTITY_INQUIRY (object);

        g_clear_object (&self->priv->identity);
        g_clear_pointer (&self->priv->name, (GDestroyNotify) g_free);
        g_clear_pointer (&self->priv->banner, (GDestroyNotify) g_free);

        g_list_foreach (self->priv->queries,
                        (GFunc)
                        gsd_kerberos_identity_query_free,
                        NULL);
        g_clear_pointer (&self->priv->queries, (GDestroyNotify) g_list_free);
}

static void
gsd_kerberos_identity_inquiry_finalize (GObject *object)
{
        G_OBJECT_CLASS (gsd_kerberos_identity_inquiry_parent_class)->finalize (object);
}

static void
gsd_kerberos_identity_inquiry_class_init (GsdKerberosIdentityInquiryClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->dispose = gsd_kerberos_identity_inquiry_dispose;
        object_class->finalize = gsd_kerberos_identity_inquiry_finalize;

        g_type_class_add_private (klass, sizeof (GsdKerberosIdentityInquiryPrivate));
}

static void
gsd_kerberos_identity_inquiry_init (GsdKerberosIdentityInquiry *self)
{
        self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
                                                  GSD_TYPE_KERBEROS_IDENTITY_INQUIRY,
                                                  GsdKerberosIdentityInquiryPrivate);
}

GsdIdentityInquiry *
gsd_kerberos_identity_inquiry_new (GsdKerberosIdentity *identity,
                                   const char          *name,
                                   const char          *banner,
                                   krb5_prompt          prompts[],
                                   int                  number_of_prompts)
{
        GObject                    *object;
        GsdIdentityInquiry         *inquiry;
        GsdKerberosIdentityInquiry *self;
        GError                     *error;
        int                         i;

        g_return_val_if_fail (GSD_IS_KERBEROS_IDENTITY (identity), NULL);
        g_return_val_if_fail (number_of_prompts > 0, NULL);

        object = g_object_new (GSD_TYPE_KERBEROS_IDENTITY_INQUIRY, NULL);

        inquiry = GSD_IDENTITY_INQUIRY (object);
        self = GSD_KERBEROS_IDENTITY_INQUIRY (object);

        /* FIXME: make these construct properties */
        self->priv->identity = g_object_ref (identity);
        self->priv->name = g_strdup (name);
        self->priv->banner = g_strdup (banner);

        self->priv->number_of_queries = 0;
        for (i = 0; i < number_of_prompts; i++) {
                GsdKerberosIdentityQuery *query;

                query = gsd_kerberos_identity_query_new (inquiry, &prompts[i]);

                self->priv->queries = g_list_prepend (self->priv->queries, query);
                self->priv->number_of_queries++;
        }
        self->priv->queries = g_list_reverse (self->priv->queries);

        self->priv->number_of_unanswered_queries = self->priv->number_of_queries;

        error = NULL;
        if (!g_initable_init (G_INITABLE (self), NULL, &error)) {
                g_debug ("%s", error->message);
                g_error_free (error);
                g_object_unref (self);
                return NULL;
        }

        return inquiry;
}

static GsdIdentity *
gsd_kerberos_identity_inquiry_get_identity (GsdIdentityInquiry *inquiry)
{
        GsdKerberosIdentityInquiry *self;

        g_return_val_if_fail (GSD_IS_KERBEROS_IDENTITY_INQUIRY (inquiry), NULL);

        self = GSD_KERBEROS_IDENTITY_INQUIRY (inquiry);

        return self->priv->identity;
}

static char *
gsd_kerberos_identity_inquiry_get_name (GsdIdentityInquiry *inquiry)
{
        GsdKerberosIdentityInquiry *self;

        g_return_val_if_fail (GSD_IS_KERBEROS_IDENTITY_INQUIRY (inquiry), NULL);

        self = GSD_KERBEROS_IDENTITY_INQUIRY (inquiry);

        return g_strdup (self->priv->name);
}

static char *
gsd_kerberos_identity_inquiry_get_banner (GsdIdentityInquiry *inquiry)
{
        GsdKerberosIdentityInquiry *self;

        g_return_val_if_fail (GSD_IS_KERBEROS_IDENTITY_INQUIRY (inquiry), NULL);

        self = GSD_KERBEROS_IDENTITY_INQUIRY (inquiry);

        return g_strdup (self->priv->banner);
}

static gboolean
gsd_kerberos_identity_inquiry_is_complete (GsdIdentityInquiry *inquiry)
{
        GsdKerberosIdentityInquiry *self;

        g_return_val_if_fail (GSD_IS_KERBEROS_IDENTITY_INQUIRY (inquiry), FALSE);

        self = GSD_KERBEROS_IDENTITY_INQUIRY (inquiry);

        return self->priv->number_of_unanswered_queries == 0;
}

static void
gsd_kerberos_identity_inquiry_mark_query_answered (GsdKerberosIdentityInquiry *self,
                                                   GsdKerberosIdentityQuery   *query)
{
        if (query->is_answered) {
                return;
        }

        query->is_answered = TRUE;
        self->priv->number_of_unanswered_queries--;

        if (self->priv->number_of_unanswered_queries == 0) {
                _gsd_identity_inquiry_emit_complete (GSD_IDENTITY_INQUIRY (self));
        }
}

static void
gsd_kerberos_identity_inquiry_answer_query (GsdIdentityInquiry *inquiry,
                                            GsdIdentityQuery   *query,
                                            const char         *answer)
{
        GsdKerberosIdentityInquiry *self;
        GsdKerberosIdentityQuery   *kerberos_query = (GsdKerberosIdentityQuery *) query;

        g_return_if_fail (GSD_IS_KERBEROS_IDENTITY_INQUIRY (inquiry));
        g_return_if_fail (inquiry == kerberos_query->inquiry);
        g_return_if_fail (!gsd_kerberos_identity_inquiry_is_complete (inquiry));

        self = GSD_KERBEROS_IDENTITY_INQUIRY (inquiry);

        inquiry = kerberos_query->inquiry;

        strncpy (kerberos_query->kerberos_prompt->reply->data,
                 answer,
                 kerberos_query->kerberos_prompt->reply->length);
        kerberos_query->kerberos_prompt->reply->length = (unsigned int) strlen (kerberos_query->kerberos_prompt->reply->data);

        gsd_kerberos_identity_inquiry_mark_query_answered (self, kerberos_query);
}

static void
gsd_kerberos_identity_inquiry_iter_init (GsdIdentityInquiryIter *iter,
                                         GsdIdentityInquiry     *inquiry)
{
        GsdKerberosIdentityInquiry *self = GSD_KERBEROS_IDENTITY_INQUIRY (inquiry);

        iter->data = self->priv->queries;
}

static GsdIdentityQuery *
gsd_kerberos_identity_inquiry_iter_next (GsdIdentityInquiryIter *iter,
                                         GsdIdentityInquiry     *inquiry)
{
        GsdIdentityQuery *query;
        GList            *node;

        node = iter->data;

        if (node == NULL) {
                return NULL;
        }

        query = (GsdIdentityQuery *) node->data;

        node = node->next;

        iter->data = node;

        return query;
}

static GsdIdentityQueryMode
gsd_kerberos_identity_query_get_mode (GsdIdentityInquiry *inquiry,
                                      GsdIdentityQuery   *query)
{
        GsdKerberosIdentityQuery *kerberos_query = (GsdKerberosIdentityQuery *) query;

        g_return_val_if_fail (GSD_IS_KERBEROS_IDENTITY_INQUIRY (inquiry), GSD_KERBEROS_IDENTITY_QUERY_MODE_INVISIBLE);
        g_return_val_if_fail (inquiry == kerberos_query->inquiry, GSD_KERBEROS_IDENTITY_QUERY_MODE_INVISIBLE);

        if (kerberos_query->kerberos_prompt->hidden) {
                return GSD_KERBEROS_IDENTITY_QUERY_MODE_INVISIBLE;
        } else {
                return GSD_KERBEROS_IDENTITY_QUERY_MODE_VISIBLE;
        }
}

static char *
gsd_kerberos_identity_query_get_prompt (GsdIdentityInquiry *inquiry,
                                        GsdIdentityQuery   *query)
{
        GsdKerberosIdentityQuery *kerberos_query = (GsdKerberosIdentityQuery *) query;

        g_return_val_if_fail (GSD_IS_KERBEROS_IDENTITY_INQUIRY (inquiry), GSD_KERBEROS_IDENTITY_QUERY_MODE_INVISIBLE);
        g_return_val_if_fail (inquiry == kerberos_query->inquiry, NULL);

        return g_strdup (kerberos_query->kerberos_prompt->prompt);
}

static gboolean
gsd_kerberos_identity_query_is_answered (GsdIdentityInquiry *inquiry,
                                         GsdIdentityQuery   *query)
{
        GsdKerberosIdentityQuery *kerberos_query = (GsdKerberosIdentityQuery *) query;

        g_return_val_if_fail (GSD_IS_KERBEROS_IDENTITY_INQUIRY (inquiry), GSD_KERBEROS_IDENTITY_QUERY_MODE_INVISIBLE);
        g_return_val_if_fail (inquiry == kerberos_query->inquiry, FALSE);

        return kerberos_query->is_answered;
}

static void
identity_inquiry_interface_init (GsdIdentityInquiryInterface *interface)
{
        interface->get_identity = gsd_kerberos_identity_inquiry_get_identity;
        interface->get_name = gsd_kerberos_identity_inquiry_get_name;
        interface->get_banner = gsd_kerberos_identity_inquiry_get_banner;
        interface->is_complete = gsd_kerberos_identity_inquiry_is_complete;
        interface->answer_query = gsd_kerberos_identity_inquiry_answer_query;
        interface->iter_init = gsd_kerberos_identity_inquiry_iter_init;
        interface->iter_next = gsd_kerberos_identity_inquiry_iter_next;
        interface->get_mode = gsd_kerberos_identity_query_get_mode;
        interface->get_prompt = gsd_kerberos_identity_query_get_prompt;
        interface->is_answered = gsd_kerberos_identity_query_is_answered;
}
