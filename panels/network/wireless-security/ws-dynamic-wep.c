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

#include "ws-dynamic-wep.h"
#include "wireless-security.h"
#include "eap-method.h"

struct _WirelessSecurityDynamicWEP {
	WirelessSecurity parent;

	GtkSizeGroup *size_group;
};

static void
destroy (WirelessSecurity *parent)
{
	WirelessSecurityDynamicWEP *sec = (WirelessSecurityDynamicWEP *) parent;

	if (sec->size_group)
		g_object_unref (sec->size_group);
}

static GtkWidget *
get_widget (WirelessSecurity *parent)
{
	return GTK_WIDGET (gtk_builder_get_object (parent->builder, "dynamic_wep_notebook"));
}

static gboolean
validate (WirelessSecurity *parent, GError **error)
{
	return ws_802_1x_validate (GTK_COMBO_BOX (gtk_builder_get_object (parent->builder, "dynamic_wep_auth_combo")), error);
}

static void
add_to_size_group (WirelessSecurity *parent, GtkSizeGroup *group)
{
	WirelessSecurityDynamicWEP *sec = (WirelessSecurityDynamicWEP *) parent;

	if (sec->size_group)
		g_object_unref (sec->size_group);
	sec->size_group = g_object_ref (group);

	ws_802_1x_add_to_size_group (sec->size_group,
	                             GTK_LABEL (gtk_builder_get_object (parent->builder, "dynamic_wep_auth_label")),
	                             GTK_COMBO_BOX (gtk_builder_get_object (parent->builder, "dynamic_wep_auth_combo")));
}

static void
fill_connection (WirelessSecurity *parent, NMConnection *connection)
{
	NMSettingWirelessSecurity *s_wireless_sec;

	ws_802_1x_fill_connection (GTK_COMBO_BOX (gtk_builder_get_object (parent->builder, "dynamic_wep_auth_combo")), connection);

	s_wireless_sec = nm_connection_get_setting_wireless_security (connection);
	g_assert (s_wireless_sec);

	g_object_set (s_wireless_sec, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "ieee8021x", NULL);
}

static void
auth_combo_changed_cb (GtkWidget *combo, gpointer user_data)
{
	WirelessSecurity *parent = WIRELESS_SECURITY (user_data);
	WirelessSecurityDynamicWEP *sec = (WirelessSecurityDynamicWEP *) parent;

	ws_802_1x_auth_combo_changed (combo,
	                              parent,
	                              GTK_BOX (gtk_builder_get_object (parent->builder, "dynamic_wep_method_vbox")),
	                              sec->size_group);
}

WirelessSecurityDynamicWEP *
ws_dynamic_wep_new (NMConnection *connection,
                    gboolean is_editor,
                    gboolean secrets_only)
{
	WirelessSecurity *parent;

	parent = wireless_security_init (sizeof (WirelessSecurityDynamicWEP),
	                                 get_widget,
	                                 validate,
	                                 add_to_size_group,
	                                 fill_connection,
	                                 destroy,
	                                 "/org/gnome/ControlCenter/network/ws-dynamic-wep.ui");
	if (!parent)
		return NULL;

	wireless_security_set_adhoc_compatible (parent, FALSE);
	wireless_security_set_hotspot_compatible (parent, FALSE);

	ws_802_1x_auth_combo_init (parent,
	                           GTK_COMBO_BOX (gtk_builder_get_object (parent->builder, "dynamic_wep_auth_combo")),
	                           GTK_LABEL (gtk_builder_get_object (parent->builder, "dynamic_wep_auth_label")),
	                           (GCallback) auth_combo_changed_cb,
	                           connection,
	                           is_editor,
	                           secrets_only);
	auth_combo_changed_cb (GTK_WIDGET (gtk_builder_get_object (parent->builder, "dynamic_wep_auth_combo")), (gpointer) parent);

	return (WirelessSecurityDynamicWEP *) parent;
}

