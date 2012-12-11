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

#include <glib/gi18n.h>
#include <ctype.h>
#include <string.h>
#include <nm-setting-wireless.h>

#include "wireless-security.h"
#include "eap-method.h"

struct _WirelessSecurityWPAEAP {
	WirelessSecurity parent;

	GtkSizeGroup *size_group;
};


static void
destroy (WirelessSecurity *parent)
{
	WirelessSecurityWPAEAP *sec = (WirelessSecurityWPAEAP *) parent;

	if (sec->size_group)
		g_object_unref (sec->size_group);
}

static gboolean
validate (WirelessSecurity *parent, const GByteArray *ssid)
{
	return ws_802_1x_validate (parent, "wpa_eap_auth_combo");
}

static void
add_to_size_group (WirelessSecurity *parent, GtkSizeGroup *group)
{
	WirelessSecurityWPAEAP *sec = (WirelessSecurityWPAEAP *) parent;

	if (sec->size_group)
		g_object_unref (sec->size_group);
	sec->size_group = g_object_ref (group);

	ws_802_1x_add_to_size_group (parent,
	                             sec->size_group,
	                             "wpa_eap_auth_label",
	                             "wpa_eap_auth_combo");
}

static void
fill_connection (WirelessSecurity *parent, NMConnection *connection)
{
	NMSettingWirelessSecurity *s_wireless_sec;

	ws_802_1x_fill_connection (parent, "wpa_eap_auth_combo", connection);

	s_wireless_sec = nm_connection_get_setting_wireless_security (connection);
	g_assert (s_wireless_sec);

	g_object_set (s_wireless_sec, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-eap", NULL);
}

static void
auth_combo_changed_cb (GtkWidget *combo, gpointer user_data)
{
	WirelessSecurity *parent = WIRELESS_SECURITY (user_data);
	WirelessSecurityWPAEAP *sec = (WirelessSecurityWPAEAP *) parent;

	ws_802_1x_auth_combo_changed (combo,
	                              parent,
	                              "wpa_eap_method_vbox",
	                              sec->size_group);
}

static GtkWidget *
nag_user (WirelessSecurity *parent)
{
	return ws_802_1x_nag_user (parent, "wpa_eap_auth_combo");
}

static void
update_secrets (WirelessSecurity *parent, NMConnection *connection)
{
	ws_802_1x_update_secrets (parent, "wpa_eap_auth_combo", connection);
}

WirelessSecurityWPAEAP *
ws_wpa_eap_new (NMConnection *connection,
                gboolean is_editor,
                gboolean secrets_only)
{
	WirelessSecurity *parent;
	GtkWidget *widget;

	parent = wireless_security_init (sizeof (WirelessSecurityWPAEAP),
	                                 validate,
	                                 add_to_size_group,
	                                 fill_connection,
	                                 update_secrets,
	                                 destroy,
	                                 UIDIR "/ws-wpa-eap.ui",
	                                 "wpa_eap_notebook",
	                                 NULL);
	if (!parent)
		return NULL;

	parent->nag_user = nag_user;
	parent->adhoc_compatible = FALSE;

	widget = ws_802_1x_auth_combo_init (parent,
	                                    "wpa_eap_auth_combo",
	                                    "wpa_eap_auth_label",
	                                    (GCallback) auth_combo_changed_cb,
	                                    connection,
	                                    is_editor,
	                                    secrets_only);
	auth_combo_changed_cb (widget, parent);

	return (WirelessSecurityWPAEAP *) parent;
}

