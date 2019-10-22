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

#include <string.h>

#include "ws-wep-key.h"
#include "wireless-security.h"
#include "utils.h"
#include "helpers.h"
#include "nma-ui-utils.h"

struct _WirelessSecurityWEPKey {
	WirelessSecurity parent;

	GtkComboBox    *auth_method_combo;
	GtkLabel       *auth_method_label;
	GtkGrid        *grid;
	GtkEntry       *key_entry;
	GtkComboBox    *key_index_combo;
	GtkLabel       *key_index_label;
	GtkLabel       *key_label;
	GtkCheckButton *show_key_check;

	gboolean editing_connection;
	const char *password_flags_name;

	NMWepKeyType type;
	char keys[4][65];
	guint8 cur_index;
};

static void
show_toggled_cb (WirelessSecurityWEPKey *self)
{
	gboolean visible;

	visible = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->show_key_check));
	gtk_entry_set_visibility (self->key_entry, visible);
}

static void
key_index_combo_changed_cb (WirelessSecurityWEPKey *self)
{
	const char *key;
	int key_index;

	/* Save WEP key for old key index */
	key = gtk_entry_get_text (self->key_entry);
	if (key)
		g_strlcpy (self->keys[self->cur_index], key, sizeof (self->keys[self->cur_index]));
	else
		memset (self->keys[self->cur_index], 0, sizeof (self->keys[self->cur_index]));

	key_index = gtk_combo_box_get_active (self->key_index_combo);
	g_return_if_fail (key_index <= 3);
	g_return_if_fail (key_index >= 0);

	/* Populate entry with key from new index */
	gtk_entry_set_text (self->key_entry, self->keys[key_index]);
	self->cur_index = key_index;

	wireless_security_notify_changed ((WirelessSecurity *) self);
}

static void
destroy (WirelessSecurity *parent)
{
	WirelessSecurityWEPKey *self = (WirelessSecurityWEPKey *) parent;
	int i;

	for (i = 0; i < 4; i++)
		memset (self->keys[i], 0, sizeof (self->keys[i]));
}

static GtkWidget *
get_widget (WirelessSecurity *parent)
{
	WirelessSecurityWEPKey *self = (WirelessSecurityWEPKey *) parent;
	return GTK_WIDGET (self->grid);
}

static gboolean
validate (WirelessSecurity *parent, GError **error)
{
	WirelessSecurityWEPKey *self = (WirelessSecurityWEPKey *) parent;
	const char *key;
	int i;

	key = gtk_entry_get_text (self->key_entry);
	if (!key) {
		widget_set_error (GTK_WIDGET (self->key_entry));
		g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("missing wep-key"));
		return FALSE;
	}

	if (self->type == NM_WEP_KEY_TYPE_KEY) {
		if ((strlen (key) == 10) || (strlen (key) == 26)) {
			for (i = 0; i < strlen (key); i++) {
				if (!g_ascii_isxdigit (key[i])) {
					widget_set_error (GTK_WIDGET (self->key_entry));
					g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC, _("invalid wep-key: key with a length of %zu must contain only hex-digits"), strlen (key));
					return FALSE;
				}
			}
		} else if ((strlen (key) == 5) || (strlen (key) == 13)) {
			for (i = 0; i < strlen (key); i++) {
				if (!g_ascii_isprint (key[i])) {
					widget_set_error (GTK_WIDGET (self->key_entry));
					g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC, _("invalid wep-key: key with a length of %zu must contain only ascii characters"), strlen (key));
					return FALSE;
				}
			}
		} else {
			widget_set_error (GTK_WIDGET (self->key_entry));
			g_set_error (error, NMA_ERROR, NMA_ERROR_GENERIC, _("invalid wep-key: wrong key length %zu. A key must be either of length 5/13 (ascii) or 10/26 (hex)"), strlen (key));
			return FALSE;
		}
	} else if (self->type == NM_WEP_KEY_TYPE_PASSPHRASE) {
		if (!*key || (strlen (key) > 64)) {
			widget_set_error (GTK_WIDGET (self->key_entry));
			if (!*key)
				g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("invalid wep-key: passphrase must be non-empty"));
			else
				g_set_error_literal (error, NMA_ERROR, NMA_ERROR_GENERIC, _("invalid wep-key: passphrase must be shorter than 64 characters"));
			return FALSE;
		}
	}
	widget_unset_error (GTK_WIDGET (self->key_entry));

	return TRUE;
}

static void
add_to_size_group (WirelessSecurity *parent, GtkSizeGroup *group)
{
	WirelessSecurityWEPKey *self = (WirelessSecurityWEPKey *) parent;
	gtk_size_group_add_widget (group, GTK_WIDGET (self->auth_method_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->key_label));
	gtk_size_group_add_widget (group, GTK_WIDGET (self->key_index_label));
}

static void
fill_connection (WirelessSecurity *parent, NMConnection *connection)
{
	WirelessSecurityWEPKey *self = (WirelessSecurityWEPKey *) parent;
	NMSettingWirelessSecurity *s_wsec;
	NMSettingSecretFlags secret_flags;
	gint auth_alg;
	const char *key;
	int i;

	auth_alg = gtk_combo_box_get_active (self->auth_method_combo);

	key = gtk_entry_get_text (self->key_entry);
	g_strlcpy (self->keys[self->cur_index], key, sizeof (self->keys[self->cur_index]));

	/* Blow away the old security setting by adding a clear one */
	s_wsec = (NMSettingWirelessSecurity *) nm_setting_wireless_security_new ();
	nm_connection_add_setting (connection, (NMSetting *) s_wsec);

	g_object_set (s_wsec,
	              NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "none",
	              NM_SETTING_WIRELESS_SECURITY_WEP_TX_KEYIDX, self->cur_index,
	              NM_SETTING_WIRELESS_SECURITY_AUTH_ALG, (auth_alg == 1) ? "shared" : "open",
	              NM_SETTING_WIRELESS_SECURITY_WEP_KEY_TYPE, self->type,
	              NULL);

	for (i = 0; i < 4; i++) {
		if (strlen (self->keys[i]))
			nm_setting_wireless_security_set_wep_key (s_wsec, i, self->keys[i]);
	}

	/* Save WEP_KEY_FLAGS to the connection */
	secret_flags = nma_utils_menu_to_secret_flags (GTK_WIDGET (self->key_entry));
	g_object_set (s_wsec, NM_SETTING_WIRELESS_SECURITY_WEP_KEY_FLAGS, secret_flags, NULL);

	/* Update secret flags and popup when editing the connection */
	if (self->editing_connection)
		nma_utils_update_password_storage (GTK_WIDGET (self->key_entry), secret_flags,
		                                   NM_SETTING (s_wsec), self->password_flags_name);
}

static void
wep_entry_filter_cb (WirelessSecurityWEPKey *self,
                     gchar *text,
                     gint length,
                     gint *position)
{
	if (self->type == NM_WEP_KEY_TYPE_KEY) {
		int i, count = 0;
		g_autofree gchar *result = g_new (gchar, length+1);

		for (i = 0; i < length; i++) {
			if (g_ascii_isprint (text[i]))
				result[count++] = text[i];
		}
		result[count] = 0;

		if (count > 0) {
			g_signal_handlers_block_by_func (self->key_entry, G_CALLBACK (wep_entry_filter_cb), self);
			gtk_editable_insert_text (GTK_EDITABLE (self->key_entry), result, count, position);
			g_signal_handlers_unblock_by_func (self->key_entry, G_CALLBACK (wep_entry_filter_cb), self);
		}
		g_signal_stop_emission_by_name (self->key_entry, "insert-text");
	}
}

static void
update_secrets (WirelessSecurity *parent, NMConnection *connection)
{
	WirelessSecurityWEPKey *self = (WirelessSecurityWEPKey *) parent;
	NMSettingWirelessSecurity *s_wsec;
	const char *tmp;
	int i;

	s_wsec = nm_connection_get_setting_wireless_security (connection);
	for (i = 0; s_wsec && i < 4; i++) {
		tmp = nm_setting_wireless_security_get_wep_key (s_wsec, i);
		if (tmp)
			g_strlcpy (self->keys[i], tmp, sizeof (self->keys[i]));
	}

	if (strlen (self->keys[self->cur_index]))
		gtk_entry_set_text (self->key_entry, self->keys[self->cur_index]);
}

static void
changed_cb (WirelessSecurityWEPKey *self)
{
	wireless_security_notify_changed ((WirelessSecurity *) self);
}

WirelessSecurityWEPKey *
ws_wep_key_new (NMConnection *connection,
                NMWepKeyType type,
                gboolean adhoc_create,
                gboolean secrets_only)
{
	WirelessSecurity *parent;
	WirelessSecurityWEPKey *self;
	NMSettingWirelessSecurity *s_wsec = NULL;
	NMSetting *setting = NULL;
	guint8 default_key_idx = 0;
	gboolean is_adhoc = adhoc_create;
	gboolean is_shared_key = FALSE;

	parent = wireless_security_init (sizeof (WirelessSecurityWEPKey),
	                                 get_widget,
	                                 validate,
	                                 add_to_size_group,
	                                 fill_connection,
	                                 destroy,
	                                 "/org/gnome/ControlCenter/network/ws-wep-key.ui");
	if (!parent)
		return NULL;
	self = (WirelessSecurityWEPKey *) parent;

	self->auth_method_combo = GTK_COMBO_BOX (gtk_builder_get_object (parent->builder, "auth_method_combo"));
	self->auth_method_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "auth_method_label"));
	self->grid = GTK_GRID (gtk_builder_get_object (parent->builder, "grid"));
	self->key_entry = GTK_ENTRY (gtk_builder_get_object (parent->builder, "key_entry"));
	self->key_index_combo = GTK_COMBO_BOX (gtk_builder_get_object (parent->builder, "key_index_combo"));
	self->key_index_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "key_index_label"));
	self->key_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "key_label"));
	self->show_key_check = GTK_CHECK_BUTTON (gtk_builder_get_object (parent->builder, "show_key_check"));

	self->editing_connection = secrets_only ? FALSE : TRUE;
	self->password_flags_name = NM_SETTING_WIRELESS_SECURITY_WEP_KEY0;
	self->type = type;

	gtk_entry_set_width_chars (self->key_entry, 28);

	/* Create password-storage popup menu for password entry under entry's secondary icon */
	if (connection)
		setting = (NMSetting *) nm_connection_get_setting_wireless_security (connection);
	nma_utils_setup_password_storage (GTK_WIDGET (self->key_entry), 0, setting, self->password_flags_name,
	                                  FALSE, secrets_only);

	if (connection) {
		NMSettingWireless *s_wireless;
		const char *mode, *auth_alg;

		s_wireless = nm_connection_get_setting_wireless (connection);
		mode = s_wireless ? nm_setting_wireless_get_mode (s_wireless) : NULL;
		if (mode && !strcmp (mode, "adhoc"))
			is_adhoc = TRUE;

		s_wsec = nm_connection_get_setting_wireless_security (connection);
		if (s_wsec) {
			auth_alg = nm_setting_wireless_security_get_auth_alg (s_wsec);
			if (auth_alg && !strcmp (auth_alg, "shared"))
				is_shared_key = TRUE;
		}
	}

	g_signal_connect_swapped (self->key_entry, "changed", G_CALLBACK (changed_cb), self);
	g_signal_connect_swapped (self->key_entry, "insert-text", G_CALLBACK (wep_entry_filter_cb), self);
	if (self->type == NM_WEP_KEY_TYPE_KEY)
		gtk_entry_set_max_length (self->key_entry, 26);
	else if (self->type == NM_WEP_KEY_TYPE_PASSPHRASE)
		gtk_entry_set_max_length (self->key_entry, 64);

	if (connection && s_wsec)
		default_key_idx = nm_setting_wireless_security_get_wep_tx_keyidx (s_wsec);

	gtk_combo_box_set_active (self->key_index_combo, default_key_idx);
	self->cur_index = default_key_idx;
	g_signal_connect_swapped (self->key_index_combo, "changed", G_CALLBACK (key_index_combo_changed_cb), self);

	/* Key index is useless with adhoc networks */
	if (is_adhoc || secrets_only) {
		gtk_widget_hide (GTK_WIDGET (self->key_index_combo));
		gtk_widget_hide (GTK_WIDGET (self->key_index_label));
	}

	/* Fill the key entry with the key for that index */
	if (connection)
		update_secrets (WIRELESS_SECURITY (self), connection);

	g_signal_connect_swapped (self->show_key_check, "toggled", G_CALLBACK (show_toggled_cb), self);

	gtk_combo_box_set_active (self->auth_method_combo, is_shared_key ? 1 : 0);

	g_signal_connect_swapped (self->auth_method_combo, "changed", G_CALLBACK (changed_cb), self);

	/* Don't show auth method for adhoc (which always uses open-system) or
	 * when in "simple" mode.
	 */
	if (is_adhoc || secrets_only) {
		/* Ad-Hoc connections can't use Shared Key auth */
		if (is_adhoc)
			gtk_combo_box_set_active (self->auth_method_combo, 0);
		gtk_widget_hide (GTK_WIDGET (self->auth_method_combo));
		gtk_widget_hide (GTK_WIDGET (self->auth_method_label));
	}

	return self;
}

