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

	GtkComboBox *auth_combo;
	GtkLabel    *auth_label;
	GtkGrid     *grid;
	GtkBox      *method_box;

	GtkSizeGroup *size_group;
};


static void
destroy (WirelessSecurity *parent)
{
	WirelessSecurityWPAEAP *self = (WirelessSecurityWPAEAP *) parent;

	g_clear_object (&self->size_group);
}

static GtkWidget *
get_widget (WirelessSecurity *parent)
{
	WirelessSecurityWPAEAP *self = (WirelessSecurityWPAEAP *) parent;
	return GTK_WIDGET (self->grid);
}

static gboolean
validate (WirelessSecurity *parent, GError **error)
{
	WirelessSecurityWPAEAP *self = (WirelessSecurityWPAEAP *) parent;
	return eap_method_validate (ws_802_1x_auth_combo_get_eap (self->auth_combo), error);
}

static void
add_to_size_group (WirelessSecurity *parent, GtkSizeGroup *group)
{
	WirelessSecurityWPAEAP *self = (WirelessSecurityWPAEAP *) parent;

	g_clear_object (&self->size_group);
	self->size_group = g_object_ref (group);

	gtk_size_group_add_widget (self->size_group, GTK_WIDGET (self->auth_label));
	eap_method_add_to_size_group (ws_802_1x_auth_combo_get_eap (self->auth_combo), self->size_group);
}

static void
fill_connection (WirelessSecurity *parent, NMConnection *connection)
{
	WirelessSecurityWPAEAP *self = (WirelessSecurityWPAEAP *) parent;
	NMSettingWirelessSecurity *s_wireless_sec;

	ws_802_1x_fill_connection (self->auth_combo, connection);

	s_wireless_sec = nm_connection_get_setting_wireless_security (connection);
	g_assert (s_wireless_sec);

	g_object_set (s_wireless_sec, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-eap", NULL);
}

static void
auth_combo_changed_cb (WirelessSecurityWPAEAP *self)
{
	ws_802_1x_auth_combo_changed (self->auth_combo,
	                              self->method_box,
	                              self->size_group);

	wireless_security_notify_changed (WIRELESS_SECURITY (self));
}

WirelessSecurityWPAEAP *
ws_wpa_eap_new (NMConnection *connection,
                gboolean is_editor,
                gboolean secrets_only)
{
	WirelessSecurity *parent;
	WirelessSecurityWPAEAP *self;

	parent = wireless_security_init (sizeof (WirelessSecurityWPAEAP),
	                                 get_widget,
	                                 validate,
	                                 add_to_size_group,
	                                 fill_connection,
	                                 destroy,
	                                 "/org/gnome/ControlCenter/network/ws-wpa-eap.ui");
	if (!parent)
		return NULL;
	self = (WirelessSecurityWPAEAP *) parent;

	self->auth_combo = GTK_COMBO_BOX (gtk_builder_get_object (parent->builder, "auth_combo"));
	self->auth_label = GTK_LABEL (gtk_builder_get_object (parent->builder, "auth_label"));
	self->grid = GTK_GRID (gtk_builder_get_object (parent->builder, "grid"));
	self->method_box = GTK_BOX (gtk_builder_get_object (parent->builder, "method_box"));

	wireless_security_set_adhoc_compatible (parent, FALSE);
	wireless_security_set_hotspot_compatible (parent, FALSE);

	ws_802_1x_auth_combo_init (parent,
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

