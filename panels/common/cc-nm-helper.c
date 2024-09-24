/*
 * Copyright (C) 2024 Red Hat, Inc
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
 * Author(s):
 *      Felipe Borges <felipeborges@gnome.org>
 */

#include <NetworkManager.h>
#include "shell/cc-object-storage.h"

#include "cc-nm-helper.h"

gchar *
cc_get_ipv4_address (void)
{
  NMActiveConnection* connection = NULL;
  g_autoptr(NMClient) client = NULL;
  NMIPConfig *ip_config;
  const gchar *ip_address;
  GPtrArray *addresses;

  if (!cc_object_storage_has_object (CC_OBJECT_NMCLIENT)) {
    g_autoptr(NMClient) new_client = nm_client_new (NULL, NULL);
    cc_object_storage_add_object (CC_OBJECT_NMCLIENT, new_client);
  }

  client = cc_object_storage_get_object (CC_OBJECT_NMCLIENT);
  connection = nm_client_get_primary_connection (client);
  if (!connection)
    return NULL;

  ip_config = nm_active_connection_get_ip4_config (connection);
  if (!ip_config)
    return NULL;

  addresses = nm_ip_config_get_addresses (ip_config);
  if (!addresses)
    return NULL;

  ip_address = nm_ip_address_get_address (g_ptr_array_index (addresses, 0));
  if (ip_address)
    return g_strdup (ip_address);

  return NULL;
}
