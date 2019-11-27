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
 * Copyright 2009 - 2014 Red Hat, Inc.
 */

#include "helpers.h"

void
helper_fill_secret_entry (NMConnection *connection,
                          GtkEntry *entry,
                          GType setting_type,
                          HelperSecretFunc func)
{
	NMSetting *setting;
	const char *tmp;

	g_return_if_fail (connection != NULL);
	g_return_if_fail (entry != NULL);
	g_return_if_fail (func != NULL);

	setting = nm_connection_get_setting (connection, setting_type);
	if (setting) {
		tmp = (*func) (setting);
		if (tmp) {
			gtk_entry_set_text (entry, tmp);
		}
	}
}

void
wireless_security_clear_ciphers (NMConnection *connection)
{
	NMSettingWirelessSecurity *s_wireless_sec;

	g_return_if_fail (connection != NULL);

	s_wireless_sec = nm_connection_get_setting_wireless_security (connection);
	g_assert (s_wireless_sec);

	nm_setting_wireless_security_clear_protos (s_wireless_sec);
	nm_setting_wireless_security_clear_pairwise (s_wireless_sec);
	nm_setting_wireless_security_clear_groups (s_wireless_sec);
}
