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

#ifndef __GSD_IDENTITY_INQUIRY_H__
#define __GSD_IDENTITY_INQUIRY_H__

#include <stdint.h>

#include <glib.h>
#include <glib-object.h>
#include <gio/gio.h>

#include "gsd-identity.h"

G_BEGIN_DECLS

#define GSD_TYPE_IDENTITY_INQUIRY             (gsd_identity_inquiry_get_type ())
#define GSD_IDENTITY_INQUIRY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSD_TYPE_IDENTITY_INQUIRY, GsdIdentityInquiry))
#define GSD_IDENTITY_INQUIRY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GSD_TYPE_IDENTITY_INQUIRY, GsdIdentityInquiryClass))
#define GSD_IS_IDENTITY_INQUIRY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSD_TYPE_IDENTITY_INQUIRY))
#define GSD_IDENTITY_INQUIRY_GET_IFACE(obj)   (G_TYPE_INSTANCE_GET_INTERFACE((obj), GSD_TYPE_IDENTITY_INQUIRY, GsdIdentityInquiryInterface))
typedef struct _GsdIdentityInquiry            GsdIdentityInquiry;
typedef struct _GsdIdentityInquiryInterface   GsdIdentityInquiryInterface;
typedef struct _GsdIdentityInquiryIter        GsdIdentityInquiryIter;

typedef struct _GsdIdentityQuery GsdIdentityQuery;

typedef void (* GsdIdentityInquiryFunc) (GsdIdentityInquiry *inquiry,
                                         GCancellable       *cancellable,
                                         gpointer            user_data);

typedef enum
{
        GSD_IDENTITY_QUERY_MODE_INVISIBLE,
        GSD_IDENTITY_QUERY_MODE_VISIBLE
} GsdIdentityQueryMode;

struct _GsdIdentityInquiryIter
{
        gpointer data;
};

struct _GsdIdentityInquiryInterface
{
        GTypeInterface base_interface;

        GsdIdentity * (* get_identity)  (GsdIdentityInquiry *inquiry);
        char *        (* get_name)      (GsdIdentityInquiry *inquiry);
        char *        (* get_banner)    (GsdIdentityInquiry *inquiry);
        gboolean      (* is_complete)   (GsdIdentityInquiry *inquiry);
        void          (* answer_query)  (GsdIdentityInquiry *inquiry,
                                         GsdIdentityQuery   *query,
                                         const char         *answer);

        void               (* iter_init) (GsdIdentityInquiryIter *iter,
                                          GsdIdentityInquiry     *inquiry);
        GsdIdentityQuery * (* iter_next) (GsdIdentityInquiryIter *iter,
                                          GsdIdentityInquiry     *inquiry);

        GsdIdentityQueryMode (* get_mode)   (GsdIdentityInquiry *inquiry,
                                             GsdIdentityQuery   *query);
        char *               (* get_prompt) (GsdIdentityInquiry *inquiry,
                                             GsdIdentityQuery   *query);
        gboolean             (* is_answered) (GsdIdentityInquiry *inquiry,
                                              GsdIdentityQuery   *query);
};

GType        gsd_identity_inquiry_get_type     (void);

GsdIdentity *gsd_identity_inquiry_get_identity (GsdIdentityInquiry *inquiry);
char        *gsd_identity_inquiry_get_name     (GsdIdentityInquiry *inquiry);
char        *gsd_identity_inquiry_get_banner   (GsdIdentityInquiry *inquiry);
gboolean     gsd_identity_inquiry_is_complete  (GsdIdentityInquiry *inquiry);
void         gsd_identity_inquiry_answer_query (GsdIdentityInquiry *inquiry,
                                                GsdIdentityQuery   *query,
                                                const char         *answer);

void              gsd_identity_inquiry_iter_init (GsdIdentityInquiryIter *iter,
                                                  GsdIdentityInquiry     *inquiry);
GsdIdentityQuery *gsd_identity_inquiry_iter_next (GsdIdentityInquiryIter *iter, GsdIdentityInquiry     *inquiry);

GsdIdentityQueryMode  gsd_identity_query_get_mode    (GsdIdentityInquiry *inquiry,
                                                      GsdIdentityQuery   *query);
char                 *gsd_identity_query_get_prompt  (GsdIdentityInquiry *inquiry,
                                                      GsdIdentityQuery   *query);
gboolean              gsd_identity_query_is_answered (GsdIdentityInquiry *inquiry,
                                                      GsdIdentityQuery   *query);

#endif /* __GSD_IDENTITY_INQUIRY_H__ */
