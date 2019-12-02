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
 * Copyright 2007 - 2014 Red Hat, Inc.
 */

#pragma once

#include <gtk/gtk.h>
#include <NetworkManager.h>

G_BEGIN_DECLS

G_DECLARE_INTERFACE (WirelessSecurity, wireless_security, WIRELESS, SECURITY, GObject)

struct _WirelessSecurityInterface {
	GTypeInterface g_iface;

	void       (*add_to_size_group) (WirelessSecurity *sec, GtkSizeGroup *group);
	void       (*fill_connection)   (WirelessSecurity *sec, NMConnection *connection);
	gboolean   (*validate)          (WirelessSecurity *sec, GError **error);
	gboolean   (*adhoc_compatible)  (WirelessSecurity *sec);
};

gboolean wireless_security_validate (WirelessSecurity *sec, GError **error);

void wireless_security_add_to_size_group (WirelessSecurity *sec,
                                          GtkSizeGroup *group);

void wireless_security_fill_connection (WirelessSecurity *sec,
                                        NMConnection *connection);

gboolean wireless_security_adhoc_compatible (WirelessSecurity *sec);

void wireless_security_notify_changed (WirelessSecurity *sec);

G_END_DECLS
