/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "panel-common.h"


/**
 * panel_device_type_to_icon_name:
 **/
const gchar *
panel_device_type_to_icon_name (guint type)
{
	const gchar *value = NULL;
	switch (type) {
	case NM_DEVICE_TYPE_ETHERNET:
		value = "network-wired";
		break;
	case NM_DEVICE_TYPE_WIFI:
	case NM_DEVICE_TYPE_GSM:
	case NM_DEVICE_TYPE_CDMA:
	case NM_DEVICE_TYPE_BLUETOOTH:
	case NM_DEVICE_TYPE_MESH:
		value = "network-wireless";
		break;
	default:
		break;
	}
	return value;
}

/**
 * panel_device_type_to_localized_string:
 **/
const gchar *
panel_device_type_to_localized_string (guint type)
{
	const gchar *value = NULL;
	switch (type) {
	case NM_DEVICE_TYPE_UNKNOWN:
		/* TRANSLATORS: device type */
		value = _("Unknown");
		break;
	case NM_DEVICE_TYPE_ETHERNET:
		/* TRANSLATORS: device type */
		value = _("Wired");
		break;
	case NM_DEVICE_TYPE_WIFI:
		/* TRANSLATORS: device type */
		value = _("Wireless");
		break;
	case NM_DEVICE_TYPE_GSM:
	case NM_DEVICE_TYPE_CDMA:
		/* TRANSLATORS: device type */
		value = _("Mobile broadband");
		break;
	case NM_DEVICE_TYPE_BLUETOOTH:
		/* TRANSLATORS: device type */
		value = _("Bluetooth");
		break;
	case NM_DEVICE_TYPE_MESH:
		/* TRANSLATORS: device type */
		value = _("Mesh");
		break;

	default:
		break;
	}
	return value;
}

/**
 * panel_device_type_to_sortable_string:
 *
 * Try to return order of approximate connection speed.
 **/
const gchar *
panel_device_type_to_sortable_string (guint type)
{
	const gchar *value = NULL;
	switch (type) {
	case NM_DEVICE_TYPE_ETHERNET:
		value = "1";
		break;
	case NM_DEVICE_TYPE_WIFI:
		value = "2";
		break;
	case NM_DEVICE_TYPE_GSM:
	case NM_DEVICE_TYPE_CDMA:
		value = "3";
		break;
	case NM_DEVICE_TYPE_BLUETOOTH:
		value = "4";
		break;
	case NM_DEVICE_TYPE_MESH:
		value = "5";
		break;
	default:
		value = "6";
		break;
	}
	return value;
}

/**
 * panel_ap_mode_to_localized_string:
 **/
const gchar *
panel_ap_mode_to_localized_string (guint mode)
{
	const gchar *value = NULL;
	switch (mode) {
	case NM_802_11_MODE_UNKNOWN:
		/* TRANSLATORS: AP type */
		value = _("Unknown");
		break;
	case NM_802_11_MODE_ADHOC:
		/* TRANSLATORS: AP type */
		value = _("Ad-hoc");
		break;
	case NM_802_11_MODE_INFRA:
		/* TRANSLATORS: AP type */
		value = _("Intrastructure");
		break;
	default:
		break;
	}
	return value;
}

/**
 * panel_device_state_to_localized_string:
 **/
const gchar *
panel_device_state_to_localized_string (guint type)
{
	const gchar *value = NULL;
	switch (type) {
	case NM_DEVICE_STATE_UNKNOWN:
		/* TRANSLATORS: device status */
		value = _("Status unknown");
		break;
	case NM_DEVICE_STATE_UNMANAGED:
		/* TRANSLATORS: device status */
		value = _("Unmanaged");
		break;
	case NM_DEVICE_STATE_UNAVAILABLE:
		/* TRANSLATORS: device status */
		value = _("Unavailable");
		break;
	case NM_DEVICE_STATE_DISCONNECTED:
		/* TRANSLATORS: device status */
		value = _("Disconnected");
		break;
	case NM_DEVICE_STATE_PREPARE:
		/* TRANSLATORS: device status */
		value = _("Preparing connection");
		break;
	case NM_DEVICE_STATE_CONFIG:
		/* TRANSLATORS: device status */
		value = _("Configuring connection");
		break;
	case NM_DEVICE_STATE_NEED_AUTH:
		/* TRANSLATORS: device status */
		value = _("Authenticating");
		break;
	case NM_DEVICE_STATE_IP_CONFIG:
		/* TRANSLATORS: device status */
		value = _("Getting network address");
		break;
	case NM_DEVICE_STATE_ACTIVATED:
		/* TRANSLATORS: device status */
		value = _("Connected");
		break;
	case NM_DEVICE_STATE_FAILED:
		/* TRANSLATORS: device status */
		value = _("Failed to connect");
		break;
	default:
		break;
	}
	return value;
}

/**
 * panel_ipv4_to_string:
 **/
gchar *
panel_ipv4_to_string (GVariant *variant)
{
	gchar *ip_str;
	guint32 ip;

	g_variant_get (variant, "u", &ip);
	ip_str = g_strdup_printf ("%i.%i.%i.%i",
				    ip & 0x000000ff,
				   (ip & 0x0000ff00) / 0x100,
				   (ip & 0x00ff0000) / 0x10000,
				   (ip & 0xff000000) / 0x1000000);
	return ip_str;
}

/**
 * panel_ipv6_to_string:
 *
 * Formats an 'ay' variant into a IPv6 address you recognise, e.g.
 * "fe80::21c:bfff:fe81:e8de"
 **/
gchar *
panel_ipv6_to_string (GVariant *variant)
{
	gchar tmp1;
	gchar tmp2;
	guint i = 0;
	gboolean ret = FALSE;
	GString *string;

	if (g_variant_n_children (variant) != 16)
		return NULL;

	string = g_string_new ("");
	for (i=0; i<16; i+=2) {
		g_variant_get_child (variant, i+0, "y", &tmp1);
		g_variant_get_child (variant, i+1, "y", &tmp2);
		if (tmp1 == 0 && tmp2 == 0) {
			if (!ret) {
				g_string_append (string, ":");
				ret = TRUE;
			}
		} else {
			g_string_append_printf (string,
						"%x%x%x%x:",
						(tmp1 & 0xf0) / 16,
						 tmp1 & 0x0f,
						(tmp2 & 0xf0) / 16,
						 tmp2 & 0x0f);
			ret = FALSE;
		}
	}
	g_string_set_size (string, string->len - 1);
	return g_string_free (string, FALSE);
}
