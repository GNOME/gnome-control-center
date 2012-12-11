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

#include <ctype.h>
#include <string.h>
#include <nm-setting-wireless.h>

#include "wireless-security.h"
#include "helpers.h"

#define WPA_PMK_LEN 32

struct _WirelessSecurityWPAPSK {
	WirelessSecurity parent;
};

static void
show_toggled_cb (GtkCheckButton *button, WirelessSecurity *sec)
{
	GtkWidget *widget;
	gboolean visible;

	widget = GTK_WIDGET (gtk_builder_get_object (sec->builder, "wpa_psk_entry"));
	g_assert (widget);

	visible = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	gtk_entry_set_visibility (GTK_ENTRY (widget), visible);
}

static gboolean
validate (WirelessSecurity *parent, const GByteArray *ssid)
{
	GtkWidget *entry;
	const char *key;
	guint32 len;
	int i;

	entry = GTK_WIDGET (gtk_builder_get_object (parent->builder, "wpa_psk_entry"));
	g_assert (entry);

	key = gtk_entry_get_text (GTK_ENTRY (entry));
	len = strlen (key);
	if ((len < 8) || (len > 64))
		return FALSE;

	if (len == 64) {
		/* Hex PSK */
		for (i = 0; i < len; i++) {
			if (!isxdigit (key[i]))
				return FALSE;
		}
	}

	/* passphrase can be between 8 and 63 characters inclusive */

	return TRUE;
}

static void
add_to_size_group (WirelessSecurity *parent, GtkSizeGroup *group)
{
	GtkWidget *widget;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "wpa_psk_type_label"));
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "wpa_psk_label"));
	gtk_size_group_add_widget (group, widget);
}

static void
fill_connection (WirelessSecurity *parent, NMConnection *connection)
{
	GtkWidget *widget;
	const char *key;
	NMSettingWireless *s_wireless;
	NMSettingWirelessSecurity *s_wireless_sec;
	const char *mode;
	gboolean is_adhoc = FALSE;

	s_wireless = nm_connection_get_setting_wireless (connection);
	g_assert (s_wireless);

	mode = nm_setting_wireless_get_mode (s_wireless);
	if (mode && !strcmp (mode, "adhoc"))
		is_adhoc = TRUE;

	g_object_set (s_wireless, NM_SETTING_WIRELESS_SEC, NM_SETTING_WIRELESS_SECURITY_SETTING_NAME, NULL);

	/* Blow away the old security setting by adding a clear one */
	s_wireless_sec = (NMSettingWirelessSecurity *) nm_setting_wireless_security_new ();
	nm_connection_add_setting (connection, (NMSetting *) s_wireless_sec);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "wpa_psk_entry"));
	key = gtk_entry_get_text (GTK_ENTRY (widget));
	g_object_set (s_wireless_sec, NM_SETTING_WIRELESS_SECURITY_PSK, key, NULL);

	wireless_security_clear_ciphers (connection);
	if (is_adhoc) {
		/* Ad-Hoc settings as specified by the supplicant */
		g_object_set (s_wireless_sec, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-none", NULL);
		nm_setting_wireless_security_add_proto (s_wireless_sec, "wpa");
		nm_setting_wireless_security_add_pairwise (s_wireless_sec, "none");

		/* Ad-hoc can only have _one_ group cipher... default to TKIP to be more
		 * compatible for now.  Maybe we'll support selecting CCMP later.
		 */
		nm_setting_wireless_security_add_group (s_wireless_sec, "tkip");
	} else {
		g_object_set (s_wireless_sec, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-psk", NULL);

		/* Just leave ciphers and protocol empty, the supplicant will
		 * figure that out magically based on the AP IEs and card capabilities.
		 */
	}
}

static void
update_secrets (WirelessSecurity *parent, NMConnection *connection)
{
	helper_fill_secret_entry (connection,
	                          parent->builder,
	                          "wpa_psk_entry",
	                          NM_TYPE_SETTING_WIRELESS_SECURITY,
	                          (HelperSecretFunc) nm_setting_wireless_security_get_psk);
}

WirelessSecurityWPAPSK *
ws_wpa_psk_new (NMConnection *connection, gboolean secrets_only)
{
	WirelessSecurity *parent;
	WirelessSecurityWPAPSK *sec;
	GtkWidget *widget;

	parent = wireless_security_init (sizeof (WirelessSecurityWPAPSK),
	                                 validate,
	                                 add_to_size_group,
	                                 fill_connection,
	                                 update_secrets,
	                                 NULL,
	                                 UIDIR "/ws-wpa-psk.ui",
	                                 "wpa_psk_notebook",
	                                 "wpa_psk_entry");
	if (!parent)
		return NULL;

	parent->adhoc_compatible = FALSE;
	sec = (WirelessSecurityWPAPSK *) parent;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "wpa_psk_entry"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  sec);
	gtk_entry_set_width_chars (GTK_ENTRY (widget), 28);

	/* Fill secrets, if any */
	if (connection)
		update_secrets (WIRELESS_SECURITY (sec), connection);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "show_checkbutton_wpa"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "toggled",
	                  (GCallback) show_toggled_cb,
	                  sec);

	/* Hide WPA/RSN for now since this can be autodetected by NM and the
	 * supplicant when connecting to the AP.
	 */

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "wpa_psk_type_combo"));
	g_assert (widget);
	gtk_widget_hide (widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "wpa_psk_type_label"));
	g_assert (widget);
	gtk_widget_hide (widget);

	return sec;
}

