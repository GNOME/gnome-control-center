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

