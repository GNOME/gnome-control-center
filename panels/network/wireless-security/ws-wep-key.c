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
#include <nm-setting-wireless-security.h>

#include "wireless-security.h"

struct _WirelessSecurityWEPKey {
	WirelessSecurity parent;

	NMWepKeyType type;
	char keys[4][65];
	guint8 cur_index;
};

static void
show_toggled_cb (GtkCheckButton *button, WirelessSecurity *sec)
{
	GtkWidget *widget;
	gboolean visible;

	widget = GTK_WIDGET (gtk_builder_get_object (sec->builder, "wep_key_entry"));
	g_assert (widget);

	visible = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
	gtk_entry_set_visibility (GTK_ENTRY (widget), visible);
}

static void
key_index_combo_changed_cb (GtkWidget *combo, WirelessSecurity *parent)
{
	WirelessSecurityWEPKey *sec = (WirelessSecurityWEPKey *) parent;
	GtkWidget *entry;
	const char *key;
	int key_index;

	/* Save WEP key for old key index */
	entry = GTK_WIDGET (gtk_builder_get_object (parent->builder, "wep_key_entry"));
	key = gtk_entry_get_text (GTK_ENTRY (entry));
	if (key)
		strcpy (sec->keys[sec->cur_index], key);
	else
		memset (sec->keys[sec->cur_index], 0, sizeof (sec->keys[sec->cur_index]));

	key_index = gtk_combo_box_get_active (GTK_COMBO_BOX (combo));
	g_return_if_fail (key_index <= 3);
	g_return_if_fail (key_index >= 0);

	/* Populate entry with key from new index */
	gtk_entry_set_text (GTK_ENTRY (entry), sec->keys[key_index]);
	sec->cur_index = key_index;

	wireless_security_changed_cb (combo, parent);
}

static void
destroy (WirelessSecurity *parent)
{
	WirelessSecurityWEPKey *sec = (WirelessSecurityWEPKey *) parent;
	int i;

	for (i = 0; i < 4; i++)
		memset (sec->keys[i], 0, sizeof (sec->keys[i]));
}

static gboolean
validate (WirelessSecurity *parent, const GByteArray *ssid)
{
	WirelessSecurityWEPKey *sec = (WirelessSecurityWEPKey *) parent;
	GtkWidget *entry;
	const char *key;
	int i;

	entry = GTK_WIDGET (gtk_builder_get_object (parent->builder, "wep_key_entry"));
	g_assert (entry);

	key = gtk_entry_get_text (GTK_ENTRY (entry));
	if (!key)
		return FALSE;

	if (sec->type == NM_WEP_KEY_TYPE_KEY) {
		if ((strlen (key) == 10) || (strlen (key) == 26)) {
			for (i = 0; i < strlen (key); i++) {
				if (!isxdigit (key[i]))
					return FALSE;
			}
		} else if ((strlen (key) == 5) || (strlen (key) == 13)) {
			for (i = 0; i < strlen (key); i++) {
				if (!isascii (key[i]))
					return FALSE;
			}
		} else {
			return FALSE;
		}
	} else if (sec->type == NM_WEP_KEY_TYPE_PASSPHRASE) {
		if (!strlen (key) || (strlen (key) > 64))
			return FALSE;
	}

	return TRUE;
}

static void
add_to_size_group (WirelessSecurity *parent, GtkSizeGroup *group)
{
	GtkWidget *widget;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "auth_method_label"));
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "wep_key_label"));
	gtk_size_group_add_widget (group, widget);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "key_index_label"));
	gtk_size_group_add_widget (group, widget);
}

static void
fill_connection (WirelessSecurity *parent, NMConnection *connection)
{
	WirelessSecurityWEPKey *sec = (WirelessSecurityWEPKey *) parent;
	NMSettingWireless *s_wireless;
	NMSettingWirelessSecurity *s_wsec;
	GtkWidget *widget;
	gint auth_alg;
	const char *key;
	int i;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "auth_method_combo"));
	auth_alg = gtk_combo_box_get_active (GTK_COMBO_BOX (widget));

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "wep_key_entry"));
	key = gtk_entry_get_text (GTK_ENTRY (widget));
	strcpy (sec->keys[sec->cur_index], key);

	s_wireless = nm_connection_get_setting_wireless (connection);
	g_assert (s_wireless);

	g_object_set (s_wireless, NM_SETTING_WIRELESS_SEC, NM_SETTING_WIRELESS_SECURITY_SETTING_NAME, NULL);

	/* Blow away the old security setting by adding a clear one */
	s_wsec = (NMSettingWirelessSecurity *) nm_setting_wireless_security_new ();
	nm_connection_add_setting (connection, (NMSetting *) s_wsec);

	g_object_set (s_wsec,
	              NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "none",
	              NM_SETTING_WIRELESS_SECURITY_WEP_TX_KEYIDX, sec->cur_index,
	              NM_SETTING_WIRELESS_SECURITY_AUTH_ALG, (auth_alg == 1) ? "shared" : "open",
	              NM_SETTING_WIRELESS_SECURITY_WEP_KEY_TYPE, sec->type,
	              NULL);

	for (i = 0; i < 4; i++) {
		if (strlen (sec->keys[i]))
			nm_setting_wireless_security_set_wep_key (s_wsec, i, sec->keys[i]);
	}
}

static void
wep_entry_filter_cb (GtkEntry *   entry,
                     const gchar *text,
                     gint         length,
                     gint *       position,
                     gpointer     data)
{
	WirelessSecurityWEPKey *sec = (WirelessSecurityWEPKey *) data;
	GtkEditable *editable = GTK_EDITABLE (entry);
	int i, count = 0;
	gchar *result;

	result = g_malloc0 (length + 1);

	if (sec->type == NM_WEP_KEY_TYPE_KEY) {
		for (i = 0; i < length; i++) {
			if (isxdigit(text[i]) || isascii(text[i]))
				result[count++] = text[i];
		}
	} else if (sec->type == NM_WEP_KEY_TYPE_PASSPHRASE) {
		for (i = 0; i < length; i++)
			result[count++] = text[i];
	}

	if (count > 0) {
		g_signal_handlers_block_by_func (G_OBJECT (editable),
			                             G_CALLBACK (wep_entry_filter_cb),
			                             data);
		gtk_editable_insert_text (editable, result, count, position);
		g_signal_handlers_unblock_by_func (G_OBJECT (editable),
			                               G_CALLBACK (wep_entry_filter_cb),
			                               data);
	}

	g_signal_stop_emission_by_name (G_OBJECT (editable), "insert-text");
	g_free (result);
}

static void
update_secrets (WirelessSecurity *parent, NMConnection *connection)
{
	WirelessSecurityWEPKey *sec = (WirelessSecurityWEPKey *) parent;
	NMSettingWirelessSecurity *s_wsec;
	GtkWidget *widget;
	const char *tmp;
	int i;

	s_wsec = nm_connection_get_setting_wireless_security (connection);
	for (i = 0; s_wsec && i < 4; i++) {
		tmp = nm_setting_wireless_security_get_wep_key (s_wsec, i);
		if (tmp)
			strcpy (sec->keys[i], tmp);
	}

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "wep_key_entry"));
	if (strlen (sec->keys[sec->cur_index]))
		gtk_entry_set_text (GTK_ENTRY (widget), sec->keys[sec->cur_index]);
}

WirelessSecurityWEPKey *
ws_wep_key_new (NMConnection *connection,
                NMWepKeyType type,
                gboolean adhoc_create,
                gboolean secrets_only)
{
	WirelessSecurity *parent;
	WirelessSecurityWEPKey *sec;
	GtkWidget *widget;
	NMSettingWirelessSecurity *s_wsec = NULL;
	guint8 default_key_idx = 0;
	gboolean is_adhoc = adhoc_create;
	gboolean is_shared_key = FALSE;

	parent = wireless_security_init (sizeof (WirelessSecurityWEPKey),
	                                 validate,
	                                 add_to_size_group,
	                                 fill_connection,
	                                 update_secrets,
	                                 destroy,
	                                 UIDIR "/ws-wep-key.ui",
	                                 "wep_key_notebook",
	                                 "wep_key_entry");
	if (!parent)
		return NULL;
	
	sec = (WirelessSecurityWEPKey *) parent;
	sec->type = type;

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "wep_key_entry"));
	g_assert (widget);
	gtk_entry_set_width_chars (GTK_ENTRY (widget), 28);

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

	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  sec);
	g_signal_connect (G_OBJECT (widget), "insert-text",
	                  (GCallback) wep_entry_filter_cb,
	                  sec);
	if (sec->type == NM_WEP_KEY_TYPE_KEY)
		gtk_entry_set_max_length (GTK_ENTRY (widget), 26);
	else if (sec->type == NM_WEP_KEY_TYPE_PASSPHRASE)
		gtk_entry_set_max_length (GTK_ENTRY (widget), 64);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "key_index_combo"));
	if (connection && s_wsec)
		default_key_idx = nm_setting_wireless_security_get_wep_tx_keyidx (s_wsec);

	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), default_key_idx);
	sec->cur_index = default_key_idx;
	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) key_index_combo_changed_cb,
	                  sec);

	/* Key index is useless with adhoc networks */
	if (is_adhoc || secrets_only) {
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "key_index_label"));
		gtk_widget_hide (widget);
	}

	/* Fill the key entry with the key for that index */
	if (connection)
		update_secrets (WIRELESS_SECURITY (sec), connection);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "show_checkbutton_wep"));
	g_assert (widget);
	g_signal_connect (G_OBJECT (widget), "toggled",
	                  (GCallback) show_toggled_cb,
	                  sec);

	widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "auth_method_combo"));
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), is_shared_key ? 1 : 0);

	g_signal_connect (G_OBJECT (widget), "changed",
	                  (GCallback) wireless_security_changed_cb,
	                  sec);

	/* Don't show auth method for adhoc (which always uses open-system) or
	 * when in "simple" mode.
	 */
	if (is_adhoc || secrets_only) {
		/* Ad-Hoc connections can't use Shared Key auth */
		if (is_adhoc)
			gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);
		gtk_widget_hide (widget);
		widget = GTK_WIDGET (gtk_builder_get_object (parent->builder, "auth_method_label"));
		gtk_widget_hide (widget);
	}

	return sec;
}

