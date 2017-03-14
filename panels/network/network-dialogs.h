/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Giovanni Campagna <scampa.giovanni@gmail.com>
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
 */

#include <nm-client.h>
#include <nm-remote-settings.h>
#include <nm-device.h>
#include <nm-access-point.h>
#include "cc-network-panel.h"

void cc_network_panel_create_wifi_network (CcNetworkPanel   *panel,
					   NMClient         *client,
					   NMRemoteSettings *settings);

void cc_network_panel_connect_to_hidden_network (CcNetworkPanel   *panel,
						 NMClient         *client,
						 NMRemoteSettings *settings);

void cc_network_panel_connect_to_8021x_network (CcNetworkPanel   *panel,
                                                NMClient         *client,
                                                NMRemoteSettings *settings,
                                                NMDevice         *device,
                                                NMAccessPoint    *ap);

void cc_network_panel_connect_to_3g_network (CcNetworkPanel   *panel,
                                             NMClient         *client,
                                             NMRemoteSettings *settings,
                                             NMDevice         *device);
