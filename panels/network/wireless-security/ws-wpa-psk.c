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

#include "nm-default.h"

#include <ctype.h>
#include <string.h>

#include "ws-wpa-psk.h"
#include "wireless-security.h"
#include "helpers.h"
#include "nma-ui-utils.h"
#include "utils.h"

#define WPA_PMK_LEN 32

struct _WirelessSecurityWPAPSK {
	WirelessSecurity parent;

	GtkGrid        *grid;
	GtkEntry       *password_entry;
	GtkLabel       *password_label;
	GtkCheckButton *show_password_check;
	GtkComboBox    *type_combo;
	GtkLabel       *type_label;

	gboolean editing_connection;
	const char *password_flags_name;
};

static GtkWidget *
get_widget (WirelessSecurity *parent)
{
	WirelessSecurityWPAPSK *self = (WirelessSecurityWPAPSK *) parent;
	return GTK_WIDGET (self->grid);
}

static void
show_toggled_cb (WirelessSecurityWPAPSK *self)
{
	gboolean visible;

	visible = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->show_password_check));
	gtk_entry_set_visibility (self->password_entry, visible);
}

static gboolean
validate (WirelessSecurity *parent, GError **error)
{
	WirelessSecurityWPAPSK *self = (WirelessSecurityWPAPSK *) parent;
	const char *key;
	gsize len;
	int i;

	key = gtk_entry_get_text (self->password_entry);
	len = key ? strlen (key) : 0;
	if ((len < 8) || (len > 64)) {
		widget_set_error (GTK_WIDGET (self->password_entry));
		g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC, _("invalid wpa-psk: invalid key-length %zu. Must be [8,63] bytes or 64 hex digits"), len);
		return FALSE;
	}

	if (len == 64) {
		/* Hex PSK */
		for (i = 0; i < len; i++) {
			if (!isxdigit (key[i])) {
				widget_set_error (GTK_WIDGET (self->password_entry));
				g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("invalid wpa-psk: cannot interpret key with 64 bytes as hex"));
				return FALSE;
			}
		}
	}
	widget_unset_error (GTK_WIDGET (self->password_entry));

	/* passphrase can be between 8 and 63 characters inclusive */

	return TRUE;
}

static void
add_to_size_group (WirelessSecurity *parent, GtkSizeGroup *group)
{
	WirelessSecurityWPAPSK *self = (WirelessSecurityWPAPSK *) parent;
	gtk_size_group_add_widget (group, GTK_WIDGET (self->type_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->password_label));
}

static void
fill_connection (WirelessSecurity *parent, NMConnection *connection)
{
	WirelessSecurityWPAPSK *self = (WirelessSecurityWPAPSK *) parent;
	const char *key;
	NMSettingWireless *s_wireless;
	NMSettingWirelessSecurity *s_wireless_sec;
	NMSettingSecretFlags secret_flags;
	const char *mode;
	gboolean is_adhoc = FALSE;

	s_wireless = nm_connection_get_setting_wireless (connection);
	g_assert (s_wireless);

	mode = nm_setting_wireless_get_mode (s_wireless);
	if (mode && !strcmp (mode, "adhoc"))
		is_adhoc = TRUE;

	/* Blow away the old security setting by adding a clear one */
	s_wireless_sec = (NMSettingWirelessSecurity *) nm_setting_wireless_security_new ();
	nm_connection_add_setting (connection, (NMSetting *) s_wireless_sec);

	key = gtk_entry_get_text (self->password_entry);
	g_object_set (s_wireless_sec, NM_SETTING_WIRELESS_SECURITY_PSK, key, NULL);

	/* Save PSK_FLAGS to the connection */
	secret_flags = nma_utils_menu_to_secret_flags (GTK_WIDGET (self->password_entry));
	nm_setting_set_secret_flags (NM_SETTING (s_wireless_sec), NM_SETTING_WIRELESS_SECURITY_PSK,
	                             secret_flags, NULL);

	/* Update secret flags and popup when editing the connection */
	if (self->editing_connection)
		nma_utils_update_password_storage (GTK_WIDGET (self->password_entry), secret_flags,
		                                   NM_SETTING (s_wireless_sec), self->password_flags_name);

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
	WirelessSecurityWPAPSK *self = (WirelessSecurityWPAPSK *) parent;
	helper_fill_secret_entry (connection,
	                          self->password_entry,
	                          NM_TYPE_SETTING_WIRELESS_SECURITY,
	                          (HelperSecretFunc) nm_setting_wireless_security_get_psk);
}

static void
changed_cb (WirelessSecurityWPAPSK *self)
{
	wireless_security_notify_changed ((WirelessSecurity *) self);
}

WirelessSecurityWPAPSK *
ws_wpa_psk_new (NMConnection *connection, gboolean secrets_only)
{
	WirelessSecurity *parent;
	WirelessSecurityWPAPSK *self;
	NMSetting *setting = NULL;

	parent = wireless_security_init (sizeof (WirelessSecurityWPAPSK),
	                                 get_widget,
	                                 validate,
	                                 add_to_size_group,
	                                 fill_connection,
	                                 NULL,
	                                 "/org/gnome/ControlCenter/network/ws-wpa-psk.ui");
	if (!parent)
		return NULL;
	self = (WirelessSecurityWPAPSK *) parent;

	self->grid = GTK_GRID (gtk_builder_get_object (parent->builder, "grid"));
	self->password_entry = GTK_ENTRY (gtk_builder_get_object (parent->builder, "password_entry"));
	self->password_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "password_label"));
	self->show_password_check = GTK_CHECK_BUTTON (gtk_builder_get_object (parent->builder, "show_password_check"));
	self->type_combo = GTK_COMBO_BOX (gtk_builder_get_object (parent->builder, "type_combo"));
	self->type_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "type_label"));

	wireless_security_set_adhoc_compatible (parent, FALSE);

	self->editing_connection = secrets_only ? FALSE : TRUE;
	self->password_flags_name = NM_SETTING_WIRELESS_SECURITY_PSK;

	g_signal_connect_swapped (self->password_entry, "changed", G_CALLBACK (changed_cb), self);
	gtk_entry_set_width_chars (self->password_entry, 28);

	/* Create password-storage popup menu for password entry under entry's secondary icon */
	if (connection)
		setting = (NMSetting *) nm_connection_get_setting_wireless_security (connection);
	nma_utils_setup_password_storage (GTK_WIDGET (self->password_entry), 0, setting, self->password_flags_name,
	                                  FALSE, secrets_only);

	/* Fill secrets, if any */
	if (connection)
		update_secrets (WIRELESS_SECURITY (self), connection);

	g_signal_connect_swapped (self->show_password_check, "toggled", G_CALLBACK (show_toggled_cb), self);

	/* Hide WPA/RSN for now since this can be autodetected by NM and the
	 * supplicant when connecting to the AP.
	 */

	gtk_widget_hide (GTK_WIDGET (self->type_combo));
	gtk_widget_hide (GTK_WIDGET (self->type_label));

	return self;
}

