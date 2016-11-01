/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012  Red Hat, Inc,
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

#ifndef __PP_HOST_H__
#define __PP_HOST_H__

#include <glib-object.h>
#include <gio/gio.h>
#include "pp-utils.h"

G_BEGIN_DECLS

#define PP_TYPE_HOST         (pp_host_get_type ())
#define PP_HOST(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), PP_TYPE_HOST, PpHost))
#define PP_HOST_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), PP_TYPE_HOST, PpHostClass))
#define PP_IS_HOST(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), PP_TYPE_HOST))
#define PP_IS_HOST_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), PP_TYPE_HOST))
#define PP_HOST_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), PP_TYPE_HOST, PpHostClass))

#define PP_HOST_UNSET_PORT               -1
#define PP_HOST_DEFAULT_IPP_PORT        631
#define PP_HOST_DEFAULT_JETDIRECT_PORT 9100
#define PP_HOST_DEFAULT_LPD_PORT        515

typedef struct _PpHost        PpHost;
typedef struct _PpHostClass   PpHostClass;
typedef struct _PpHostPrivate PpHostPrivate;

struct _PpHost
{
  GObject        parent_instance;
  PpHostPrivate *priv;
};

struct _PpHostClass
{
  GObjectClass parent_class;

  void (*authentication_required) (PpHost *host);
};

GType          pp_host_get_type                       (void) G_GNUC_CONST;

PpHost        *pp_host_new                            (const gchar          *hostname);

void           pp_host_get_snmp_devices_async         (PpHost               *host,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);

PpDevicesList *pp_host_get_snmp_devices_finish        (PpHost               *host,
                                                       GAsyncResult         *result,
                                                       GError              **error);

void           pp_host_get_remote_cups_devices_async  (PpHost               *host,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);

PpDevicesList *pp_host_get_remote_cups_devices_finish (PpHost               *host,
                                                       GAsyncResult         *result,
                                                       GError              **error);

void           pp_host_get_jetdirect_devices_async    (PpHost               *host,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);

PpDevicesList *pp_host_get_jetdirect_devices_finish   (PpHost               *host,
                                                       GAsyncResult         *result,
                                                       GError              **error);

void           pp_host_get_lpd_devices_async          (PpHost               *host,
                                                       GCancellable         *cancellable,
                                                       GAsyncReadyCallback   callback,
                                                       gpointer              user_data);

PpDevicesList *pp_host_get_lpd_devices_finish         (PpHost               *host,
                                                       GAsyncResult         *result,
                                                       GError              **error);

G_END_DECLS

#endif /* __PP_HOST_H__ */
