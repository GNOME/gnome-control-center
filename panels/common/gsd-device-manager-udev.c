/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Red Hat
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#include "config.h"

#include <string.h>
#include <gudev/gudev.h>

#include <gdk/gdkwayland.h>
#include "gsd-device-manager-udev.h"

struct _GsdUdevDeviceManager
{
	GsdDeviceManager parent_instance;
	GHashTable *devices;
	GUdevClient *udev_client;
};

struct _GsdUdevDeviceManagerClass
{
	GsdDeviceManagerClass parent_class;
};

G_DEFINE_TYPE (GsdUdevDeviceManager, gsd_udev_device_manager, GSD_TYPE_DEVICE_MANAGER)

/* Index matches GsdDeviceType */
const gchar *udev_ids[] = {
	"ID_INPUT_MOUSE",
	"ID_INPUT_KEYBOARD",
	"ID_INPUT_TOUCHPAD",
	"ID_INPUT_TABLET",
	"ID_INPUT_TOUCHSCREEN"
};

static GsdDeviceType
udev_device_get_device_type (GUdevDevice *device)
{
	GsdDeviceType type = 0;
	gint i;

	for (i = 0; i < G_N_ELEMENTS (udev_ids); i++) {
		if (g_udev_device_get_property_as_boolean (device, udev_ids[i]))
			type |= (1 << i);
	}

	return type;
}

static gboolean
device_is_evdev (GUdevDevice *device)
{
	const gchar *device_file;

	device_file = g_udev_device_get_device_file (device);

	if (!device_file || strstr (device_file, "/event") == NULL)
		return FALSE;

	return g_udev_device_get_property_as_boolean (device, "ID_INPUT");
}

static GsdDevice *
create_device (GUdevDevice *udev_device)
{
	const gchar *vendor, *product, *name;
	guint width, height;
	GUdevDevice *parent;
	GsdDevice *device;

	parent = g_udev_device_get_parent (udev_device);
	g_assert (parent != NULL);

	name = g_udev_device_get_sysfs_attr (parent, "name");
	vendor = g_udev_device_get_property (udev_device, "ID_VENDOR_ID");
	product = g_udev_device_get_property (udev_device, "ID_MODEL_ID");
	width = g_udev_device_get_property_as_int (udev_device, "ID_INPUT_WIDTH_MM");
	height = g_udev_device_get_property_as_int (udev_device, "ID_INPUT_WIDTH_MM");

	device = g_object_new (GSD_TYPE_DEVICE,
			       "name", name,
			       "device-file", g_udev_device_get_device_file (udev_device),
			       "type", udev_device_get_device_type (udev_device),
			       "vendor-id", vendor,
			       "product-id", product,
			       "width", width,
			       "height", height,
			       NULL);

	g_object_unref (parent);

	return device;
}

static void
add_device (GsdUdevDeviceManager *manager,
	    GUdevDevice		 *udev_device)
{
	GUdevDevice *parent;
	GsdDevice *device;

	parent = g_udev_device_get_parent (udev_device);

	if (!parent)
		return;

	device = create_device (udev_device);
	g_hash_table_insert (manager->devices, g_object_ref (udev_device), device);
	g_signal_emit_by_name (manager, "device-added", device);
}

static void
remove_device (GsdUdevDeviceManager *manager,
	       GUdevDevice	    *udev_device)
{
	GsdDevice *device;

	device = g_hash_table_lookup (manager->devices, udev_device);

	if (!device)
		return;

	g_hash_table_steal (manager->devices, udev_device);
	g_signal_emit_by_name (manager, "device-removed", device);

	g_object_unref (device);
	g_object_unref (udev_device);
}

static void
udev_event_cb (GUdevClient	    *client,
	       gchar		    *action,
	       GUdevDevice	    *device,
	       GsdUdevDeviceManager *manager)
{
	if (!device_is_evdev (device))
		return;

	if (g_strcmp0 (action, "add") == 0) {
		add_device (manager, device);
	} else if (g_strcmp0 (action, "remove") == 0) {
		remove_device (manager, device);
	}
}

static void
gsd_udev_device_manager_init (GsdUdevDeviceManager *manager)
{
	const gchar *subsystems[] = { "input", NULL };
	GList *devices, *l;

	manager->devices = g_hash_table_new_full (NULL, NULL,
						  (GDestroyNotify) g_object_unref,
						  (GDestroyNotify) g_object_unref);

	manager->udev_client = g_udev_client_new (subsystems);
	g_signal_connect (manager->udev_client, "uevent",
			  G_CALLBACK (udev_event_cb), manager);

	devices = g_udev_client_query_by_subsystem (manager->udev_client,
						    subsystems[0]);

	for (l = devices; l; l = l->next) {
		GUdevDevice *device = l->data;

		if (device_is_evdev (device))
			add_device (manager, device);

		g_object_unref (device);
	}

	g_list_free (devices);
}

static void
gsd_udev_device_manager_finalize (GObject *object)
{
	GsdUdevDeviceManager *manager = GSD_UDEV_DEVICE_MANAGER (object);

	g_hash_table_destroy (manager->devices);
	g_object_unref (manager->udev_client);

	G_OBJECT_CLASS (gsd_udev_device_manager_parent_class)->finalize (object);
}

static GList *
gsd_udev_device_manager_list_devices (GsdDeviceManager *manager,
				      GsdDeviceType	type)
{
	GsdUdevDeviceManager *manager_udev = GSD_UDEV_DEVICE_MANAGER (manager);
	GsdDeviceType device_type;
	GList *devices = NULL;
	GHashTableIter iter;
	GsdDevice *device;

	g_hash_table_iter_init (&iter, manager_udev->devices);

	while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &device)) {
		device_type = gsd_device_get_device_type (device);

		if ((device_type & type) == type)
			devices = g_list_prepend (devices, device);
	}

	return devices;
}

static GsdDevice *
gsd_udev_device_manager_lookup_device (GsdDeviceManager *manager,
				       GdkDevice	*gdk_device)
{
	const gchar *node_path;
	GHashTableIter iter;
	GsdDevice *device;

	node_path = gdk_wayland_device_get_node_path (gdk_device);
	if (!node_path)
		return NULL;

	g_hash_table_iter_init (&iter, GSD_UDEV_DEVICE_MANAGER (manager)->devices);

	while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &device)) {
		if (g_strcmp0 (node_path,
			       gsd_device_get_device_file (device)) == 0) {
			return device;
		}
	}

	return NULL;
}

static void
gsd_udev_device_manager_class_init (GsdUdevDeviceManagerClass *klass)
{
	GsdDeviceManagerClass *manager_class = GSD_DEVICE_MANAGER_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gsd_udev_device_manager_finalize;
	manager_class->list_devices = gsd_udev_device_manager_list_devices;
	manager_class->lookup_device = gsd_udev_device_manager_lookup_device;
}
