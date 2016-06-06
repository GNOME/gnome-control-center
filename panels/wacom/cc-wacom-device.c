/*
 * Copyright © 2016 Red Hat, Inc.
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Carlos Garnacho <carlosg@gnome.org>
 *
 */

#include "config.h"

#include <string.h>
#include "cc-wacom-device.h"

enum {
	PROP_0,
	PROP_DEVICE,
	N_PROPS
};

GParamSpec *props[N_PROPS] = { 0 };

typedef struct _CcWacomDevice CcWacomDevice;

struct _CcWacomDevice {
	GObject parent_instance;

	GsdDevice *device;
	WacomDevice *wdevice;
};

static void cc_wacom_device_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (CcWacomDevice, cc_wacom_device, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
						cc_wacom_device_initable_iface_init))

WacomDeviceDatabase *
cc_wacom_device_database_get (void)
{
	static WacomDeviceDatabase *db = NULL;

	if (g_once_init_enter (&db)) {
		gpointer p = libwacom_database_new ();
		g_once_init_leave (&db, p);
	}

	return db;
}

static void
cc_wacom_device_init (CcWacomDevice *device)
{
}

static void
cc_wacom_device_set_property (GObject      *object,
			      guint         prop_id,
			      const GValue *value,
			      GParamSpec   *pspec)
{
	CcWacomDevice *device = CC_WACOM_DEVICE (object);

	switch (prop_id) {
	case PROP_DEVICE:
		device->device = g_value_get_object (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
cc_wacom_device_get_property (GObject    *object,
			      guint       prop_id,
			      GValue     *value,
			      GParamSpec *pspec)
{
	CcWacomDevice *device = CC_WACOM_DEVICE (object);

	switch (prop_id) {
	case PROP_DEVICE:
		g_value_set_object (value, device->device);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
cc_wacom_device_finalize (GObject *object)
{
	CcWacomDevice *device = CC_WACOM_DEVICE (object);

	g_clear_pointer (&device->wdevice, libwacom_destroy);

	G_OBJECT_CLASS (cc_wacom_device_parent_class)->finalize (object);
}

static void
cc_wacom_device_class_init (CcWacomDeviceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->set_property = cc_wacom_device_set_property;
	object_class->get_property = cc_wacom_device_get_property;
	object_class->finalize = cc_wacom_device_finalize;

	props[PROP_DEVICE] =
		g_param_spec_object ("device",
				     "device",
				     "device",
				     GSD_TYPE_DEVICE,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

	g_object_class_install_properties (object_class, N_PROPS, props);
}

static gboolean
cc_wacom_device_initable_init (GInitable     *initable,
			       GCancellable  *cancellable,
			       GError       **error)
{
	CcWacomDevice *device = CC_WACOM_DEVICE (initable);
	WacomDeviceDatabase *wacom_db;
	const gchar *node_path;

	wacom_db = cc_wacom_device_database_get ();
	node_path = gsd_device_get_device_file (device->device);
	device->wdevice = libwacom_new_from_path (wacom_db, node_path, FALSE, NULL);

	if (!device->wdevice) {
		g_set_error (error, 0, 0, "Tablet description not found");
		return FALSE;
	}

	return TRUE;
}

static void
cc_wacom_device_initable_iface_init (GInitableIface *iface)
{
	iface->init = cc_wacom_device_initable_init;
}

CcWacomDevice *
cc_wacom_device_new (GsdDevice *device)
{
	return g_initable_new (CC_TYPE_WACOM_DEVICE,
			       NULL, NULL,
			       "device", device,
			       NULL);
}

const gchar *
cc_wacom_device_get_name (CcWacomDevice *device)
{
	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), NULL);

	return libwacom_get_name (device->wdevice);
}

const gchar *
cc_wacom_device_get_icon_name (CcWacomDevice *device)
{
	WacomIntegrationFlags integration_flags;

	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), NULL);

	integration_flags = libwacom_get_integration_flags (device->wdevice);

	if (integration_flags & WACOM_DEVICE_INTEGRATED_SYSTEM) {
		return "wacom-tablet-pc";
	} else if (integration_flags & WACOM_DEVICE_INTEGRATED_DISPLAY) {
		return "wacom-tablet-cintiq";
	} else {
		return "wacom-tablet";
	}
}

gboolean
cc_wacom_device_is_reversible (CcWacomDevice *device)
{
	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), FALSE);

	return libwacom_is_reversible (device->wdevice);
}

WacomIntegrationFlags
cc_wacom_device_get_integration_flags (CcWacomDevice *device)
{
	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), 0);

	return libwacom_get_integration_flags (device->wdevice);
}

GsdDevice *
cc_wacom_device_get_device (CcWacomDevice *device)
{
	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), NULL);

	return device->device;
}

GSettings *
cc_wacom_device_get_settings (CcWacomDevice *device)
{
	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), NULL);

	return gsd_device_get_settings (device->device);
}

const gint *
cc_wacom_device_get_supported_tools (CcWacomDevice *device,
				     gint          *n_tools)
{
	*n_tools = 0;

	g_return_val_if_fail (CC_IS_WACOM_DEVICE (device), NULL);

	return libwacom_get_supported_styli (device->wdevice, n_tools);
}
