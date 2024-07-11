/* -*- mode: c; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * Copyright 2024 Red Hat Inc
 *
 * Licensed under the GNU General Public License Version 2
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
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author(s):
 *      Felipe Borges <felipeborges@gnome.org>
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-vpn-page"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <adwaita.h>
#include <glib/gi18n.h>
#include <libmm-glib.h>
#include <NetworkManager.h>

#include "cc-vpn-page.h"
#include "connection-editor/net-connection-editor.h"
#include "panels/common/cc-list-row.h"
#include "shell/cc-object-storage.h"

#include "net-vpn.h"

struct _CcVpnPage
{
  CcPanel              parent_instance;

  NMClient            *client;
  GPtrArray           *vpns;

  GtkWidget           *box_vpn;
};

G_DEFINE_TYPE (CcVpnPage, cc_vpn_page, CC_TYPE_PANEL)

static void
create_connection_cb (CcVpnPage *self,
                      NetVpn    *net_vpn)
{
  NetConnectionEditor *editor;
  NMConnection *connection = NULL;

  connection = g_object_get_data (G_OBJECT (net_vpn), "connection");
 
  editor = net_connection_editor_new (connection, NULL, NULL, self->client);
  cc_panel_push_subpage (CC_PANEL (self), ADW_NAVIGATION_PAGE (editor));
}

static gint
sort_vpns_func (GtkListBoxRow *a,
                GtkListBoxRow *b,
                gpointer user_data)
{                       
  NetVpn *vpn_a = NET_VPN (a);
  NetVpn *vpn_b = NET_VPN (b);
         
  return g_utf8_collate (nm_connection_get_id (net_vpn_get_connection (vpn_a)),
                         nm_connection_get_id (net_vpn_get_connection (vpn_b)));
}

static void
add_vpn (CcVpnPage    *self,
         NMConnection *connection)
{
  NetVpn *net_vpn;
  guint i;

  /* does already exist */
  for (i = 0; i < self->vpns->len; i++) {
    net_vpn = g_ptr_array_index (self->vpns, i);
    if (net_vpn_get_connection (net_vpn) == connection)
      return;
  }

  net_vpn = net_vpn_new (self->client, connection);

  g_object_set_data (G_OBJECT (net_vpn), "connection", connection);
  g_signal_connect_swapped (G_OBJECT (net_vpn), "activated",
                            G_CALLBACK (create_connection_cb), self);
  gtk_list_box_append (GTK_LIST_BOX (self->box_vpn), GTK_WIDGET (net_vpn));

  /* store in the devices array */
  g_ptr_array_add (self->vpns, net_vpn);
}

static void
add_connection (CcVpnPage    *self,
                NMConnection *connection)
{
  NMSettingConnection *s_con;
  const gchar *type;

  s_con = NM_SETTING_CONNECTION (nm_connection_get_setting (connection,
                                                            NM_TYPE_SETTING_CONNECTION));
  type = nm_setting_connection_get_connection_type (s_con);
  if (g_strcmp0 (type, "vpn") != 0 && g_strcmp0 (type, "wireguard") != 0)
    return;
 
  /* Don't add the libvirtd bridge to the UI */
  if (g_strcmp0 (nm_setting_connection_get_interface_name (s_con), "virbr0") == 0)
    return;

  g_debug ("add %s/%s remote connection: %s",
           type, g_type_name_from_instance ((GTypeInstance*)connection),
           nm_connection_get_path (connection));
  add_vpn (self, connection);
}

static void
client_connection_removed_cb (CcVpnPage    *self,
                              NMConnection *connection)
{
  guint i;
                                 
  for (i = 0; i < self->vpns->len; i++) {
    NetVpn *vpn = g_ptr_array_index (self->vpns, i);
    if (net_vpn_get_connection (vpn) == connection) {
      g_ptr_array_remove (self->vpns, vpn);
      gtk_list_box_remove (GTK_LIST_BOX (self->box_vpn), GTK_WIDGET (vpn));
      return;
    }
  }       
}

static void
cc_vpn_page_dispose (GObject *object)
{
  CcVpnPage *self = CC_VPN_PAGE (object);

  g_clear_pointer (&self->vpns, g_ptr_array_unref);
  g_clear_object (&self->client);

  G_OBJECT_CLASS (cc_vpn_page_parent_class)->dispose (object);
}

static void
cc_vpn_page_class_init (CcVpnPageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = cc_vpn_page_dispose;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/"
                                               "network/cc-vpn-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcVpnPage, box_vpn);

  gtk_widget_class_bind_template_callback (widget_class, create_connection_cb);
}

static void
cc_vpn_page_init (CcVpnPage *self)
{
  const GPtrArray *connections;
  guint i;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->vpns = g_ptr_array_new ();
  gtk_list_box_set_sort_func (GTK_LIST_BOX (self->box_vpn), sort_vpns_func, NULL, NULL);

  if (!cc_object_storage_has_object (CC_OBJECT_NMCLIENT)) {
                g_autoptr(NMClient) client = nm_client_new (NULL, NULL);
                cc_object_storage_add_object (CC_OBJECT_NMCLIENT, client);
   }

  self->client = cc_object_storage_get_object (CC_OBJECT_NMCLIENT);

  connections = nm_client_get_connections (self->client);
  if (connections) {
    for (i = 0; i < connections->len; i++)
      add_connection (self, connections->pdata[i]);
  }

  g_signal_connect_object (self->client, NM_CLIENT_CONNECTION_ADDED,
                           G_CALLBACK (add_connection), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (self->client, NM_CLIENT_CONNECTION_REMOVED,
                           G_CALLBACK (client_connection_removed_cb), self, G_CONNECT_SWAPPED);
}
