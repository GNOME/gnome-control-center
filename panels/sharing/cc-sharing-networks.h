/*
 * Copyright (C) 2014 Bastien Nocera <hadess@hadess.net>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef __CC_SHARING_NETWORKS_H__
#define __CC_SHARING_NETWORKS_H__

#include <gtk/gtkgrid.h>

G_BEGIN_DECLS

#define CC_TYPE_SHARING_NETWORKS             (cc_sharing_networks_get_type ())
#define CC_SHARING_NETWORKS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), CC_TYPE_SHARING_NETWORKS, CcSharingNetworks))
#define CC_SHARING_NETWORKS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), CC_TYPE_SHARING_NETWORKS, CcSharingNetworksClass))
#define CC_IS_SHARING_NETWORKS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CC_TYPE_SHARING_NETWORKS))
#define CC_IS_SHARING_NETWORKS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), CC_TYPE_SHARING_NETWORKS))
#define CC_SHARING_NETWORKS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), CC_TYPE_SHARING_NETWORKS, CcSharingNetworksClass))

typedef struct _CcSharingNetworks        CcSharingNetworks;
typedef struct _CcSharingNetworksPrivate CcSharingNetworksPrivate;
typedef struct _CcSharingNetworksClass   CcSharingNetworksClass;

struct _CcSharingNetworks
{
  GtkGrid parent_instance;

  CcSharingNetworksPrivate *priv;
};

struct _CcSharingNetworksClass
{
  GtkGridClass parent_class;
};

typedef enum {
  CC_SHARING_STATUS_UNSET,
  CC_SHARING_STATUS_OFF,
  CC_SHARING_STATUS_ENABLED,
  CC_SHARING_STATUS_ACTIVE
} CcSharingStatus;

GType          cc_sharing_networks_get_type  (void) G_GNUC_CONST;
GtkWidget    * cc_sharing_networks_new       (GDBusProxy *proxy,
					      const char *service_name);

G_END_DECLS

#endif /* __CC_SHARING_NETWORKS_H__ */
