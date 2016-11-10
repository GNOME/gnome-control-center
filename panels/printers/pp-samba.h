/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012 - 2013 Red Hat, Inc,
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Marek Kasik <mkasik@redhat.com>
 */

#ifndef __PP_SAMBA_H__
#define __PP_SAMBA_H__

#include "pp-host.h"
#include "pp-utils.h"

G_BEGIN_DECLS

#define PP_TYPE_SAMBA         (pp_samba_get_type ())
#define PP_SAMBA(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PP_TYPE_SAMBA, PpSamba))
#define PP_SAMBA_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PP_TYPE_SAMBA, PpSambaClass))
#define PP_IS_SAMBA(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PP_TYPE_SAMBA))
#define PP_IS_SAMBA_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PP_TYPE_SAMBA))
#define PP_SAMBA_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PP_TYPE_SAMBA, PpSambaClass))

typedef struct _PpSamba        PpSamba;
typedef struct _PpSambaClass   PpSambaClass;
typedef struct _PpSambaPrivate PpSambaPrivate;

struct _PpSamba
{
  PpHost          parent_instance;
  PpSambaPrivate *priv;
};

struct _PpSambaClass
{
  PpHostClass parent_class;
};

GType          pp_samba_get_type           (void) G_GNUC_CONST;

PpSamba       *pp_samba_new                (GtkWindow           *parent,
                                            const gchar         *hostname);

void           pp_samba_get_devices_async  (PpSamba             *samba,
                                            gboolean             auth_if_needed,
                                            GCancellable        *cancellable,
                                            GAsyncReadyCallback  callback,
                                            gpointer             user_data);

PpDevicesList *pp_samba_get_devices_finish (PpSamba             *samba,
                                            GAsyncResult        *result,
                                            GError             **error);

void           pp_samba_set_auth_info      (PpSamba             *samba,
                                            const gchar         *username,
                                            const gchar         *password);

G_END_DECLS

#endif /* __PP_SAMBA_H__ */
