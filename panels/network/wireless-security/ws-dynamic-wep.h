/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Applet -- allow user control over networking
 *
 * Dan Williams <dcbw@redhat.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2007 - 2010 Red Hat, Inc.
 */

#ifndef WS_DYNAMIC_WEP_H
#define WS_DYNAMIC_WEP_H

#include <NetworkManager.h>

typedef struct _WirelessSecurityDynamicWEP WirelessSecurityDynamicWEP;

WirelessSecurityDynamicWEP *ws_dynamic_wep_new (NMConnection *connection,
                                                gboolean is_editor,
                                                gboolean secrets_only);

#endif /* WS_DYNAMIC_WEP_H */

