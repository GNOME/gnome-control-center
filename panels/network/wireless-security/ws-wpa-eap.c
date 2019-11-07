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

#include "ws-wpa-eap.h"
#include "wireless-security.h"
#include "eap-method.h"

struct _WirelessSecurityWPAEAP {
	WirelessSecurity parent;

	GtkBuilder  *builder;
	GtkComboBox *auth_combo;
	GtkLabel    *auth_label;
	GtkGrid     *grid;
	GtkBox      *method_box;

	GtkSizeGroup *size_group;
};

G_DEFINE_TYPE (WirelessSecurityWPAEAP, ws_wpa_eap, wireless_security_get_type ())

static void
ws_wpa_eap_dispose (GObject *object)
{
	WirelessSecurityWPAEAP *self = WS_WPA_EAP (object);

	g_clear_object (&self->builder);
	g_clear_object (&self->size_group);

	G_OBJECT_CLASS (ws_wpa_eap_parent_class)->dispose (object);
}

static GtkWidget *
get_widget (WirelessSecurity *security)
{
	WirelessSecurityWPAEAP *self = WS_WPA_EAP (security);
	return GTK_WIDGET (self->grid);
}

static gboolean
validate (WirelessSecurity *security, GError **error)
{
	WirelessSecurityWPAEAP *self = WS_WPA_EAP (security);
	return eap_method_validate (ws_802_1x_auth_combo_get_eap (self->auth_combo), error);
}

static void
add_to_size_group (WirelessSecurity *security, GtkSizeGroup *group)
{
	WirelessSecurityWPAEAP *self = WS_WPA_EAP (security);

	g_clear_object (&self->size_group);
	self->size_group = g_object_ref (group);

	gtk_size_group_add_widget (self->size_group, GTK_WIDGET (self->auth_label));
	eap_method_add_to_size_group (ws_802_1x_auth_combo_get_eap (self->auth_combo), self->size_group);
}

static void
fill_connection (WirelessSecurity *security, NMConnection *connection)
{
	WirelessSecurityWPAEAP *self = WS_WPA_EAP (security);
	NMSettingWirelessSecurity *s_wireless_sec;

	ws_802_1x_fill_connection (self->auth_combo, connection);

	s_wireless_sec = nm_connection_get_setting_wireless_security (connection);
	g_assert (s_wireless_sec);

	g_object_set (s_wireless_sec, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-eap", NULL);
}

static gboolean
adhoc_compatible (WirelessSecurity *security)
{
	return FALSE;
}

static void
auth_combo_changed_cb (WirelessSecurityWPAEAP *self)
{
	ws_802_1x_auth_combo_changed (self->auth_combo,
	                              self->method_box,
	                              self->size_group);

	wireless_security_notify_changed (WIRELESS_SECURITY (self));
}

void
ws_wpa_eap_init (WirelessSecurityWPAEAP *self)
{
}

void
ws_wpa_eap_class_init (WirelessSecurityWPAEAPClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	WirelessSecurityClass *ws_class = WIRELESS_SECURITY_CLASS (klass);

	object_class->dispose = ws_wpa_eap_dispose;
	ws_class->get_widget = get_widget;
	ws_class->validate = validate;
	ws_class->add_to_size_group = add_to_size_group;
	ws_class->fill_connection = fill_connection;
	ws_class->adhoc_compatible = adhoc_compatible;
}

WirelessSecurityWPAEAP *
ws_wpa_eap_new (NMConnection *connection,
                gboolean is_editor,
                gboolean secrets_only)
{
	WirelessSecurityWPAEAP *self;
	g_autoptr(GError) error = NULL;

	self = g_object_new (ws_wpa_eap_get_type (), NULL);

	self->builder = gtk_builder_new ();
	if (!gtk_builder_add_from_resource (self->builder, "/org/gnome/ControlCenter/network/ws-wpa-eap.ui", &error)) {
		g_warning ("Couldn't load UI builder resource: %s", error->message);
		return NULL;
	}

	self->auth_combo = GTK_COMBO_BOX (gtk_builder_get_object (self->builder, "auth_combo"));
	self->auth_label = GTK_LABEL (gtk_builder_get_object (self->builder, "auth_label"));
	self->grid = GTK_GRID (gtk_builder_get_object (self->builder, "grid"));
	self->method_box = GTK_BOX (gtk_builder_get_object (self->builder, "method_box"));

	ws_802_1x_auth_combo_init (WIRELESS_SECURITY (self),
	                           self->auth_combo,
	                           connection,
	                           is_editor,
	                           secrets_only);

	if (secrets_only) {
		gtk_widget_hide (GTK_WIDGET (self->auth_combo));
		gtk_widget_hide (GTK_WIDGET (self->auth_label));
	}

	g_signal_connect_object (G_OBJECT (self->auth_combo), "changed", G_CALLBACK (auth_combo_changed_cb), self, G_CONNECT_SWAPPED);
	auth_combo_changed_cb (self);

	return self;
}

GtkComboBox *
ws_wpa_eap_get_auth_combo (WirelessSecurityWPAEAP *self)
{
        return self->auth_combo;
}
