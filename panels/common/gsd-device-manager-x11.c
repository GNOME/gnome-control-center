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

#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "gsd-input-helper.h"
#include "gsd-device-manager-x11.h"

struct _GsdX11DeviceManager
{
	GsdDeviceManager parent_instance;
	GdkDeviceManager *device_manager;
	GHashTable *devices;
	GHashTable *gdk_devices;
};

struct _GsdX11DeviceManagerClass
{
	GsdDeviceManagerClass parent_class;
};

GsdDevice  * gsd_x11_device_manager_lookup_gdk_device (GsdDeviceManager *manager,
						       GdkDevice	*gdk_device);

G_DEFINE_TYPE (GsdX11DeviceManager, gsd_x11_device_manager, GSD_TYPE_DEVICE_MANAGER)

static GsdDeviceType
device_get_device_type (GdkDevice *gdk_device)
{
	GdkInputSource source;

	source = gdk_device_get_source (gdk_device);

	switch (source) {
	case GDK_SOURCE_MOUSE:
		return GSD_DEVICE_TYPE_MOUSE;
	case GDK_SOURCE_PEN:
	case GDK_SOURCE_ERASER:
	case GDK_SOURCE_CURSOR:
		return GSD_DEVICE_TYPE_TABLET;
	case GDK_SOURCE_KEYBOARD:
		return GSD_DEVICE_TYPE_KEYBOARD;
	case GDK_SOURCE_TOUCHSCREEN:
		return GSD_DEVICE_TYPE_TOUCHSCREEN;
	case GDK_SOURCE_TOUCHPAD:
		return GSD_DEVICE_TYPE_TOUCHPAD;
	default:
		g_warning ("Unhandled input source %d\n", source);
	}

	return 0;
}

static GsdDevice *
create_device (GdkDevice   *gdk_device,
	       const gchar *device_file)
{
	guint width, height;
	GsdDevice *device;
	gint id;

	id = gdk_x11_device_get_id (gdk_device);
	xdevice_get_dimensions (id, &width, &height);

	device = g_object_new (GSD_TYPE_DEVICE,
			       "name", gdk_device_get_name (gdk_device),
			       "device-file", device_file,
			       "type", device_get_device_type (gdk_device),
			       "vendor-id", gdk_device_get_vendor_id (gdk_device),
			       "product-id", gdk_device_get_product_id (gdk_device),
			       "width", width,
			       "height", height,
			       NULL);
	return device;
}

static void
add_device (GsdX11DeviceManager *manager,
	    GdkDevice		*gdk_device)
{
	gchar *device_file;
	GsdDevice *device;
	gint id;

	if (gdk_device_get_device_type (gdk_device) == GDK_DEVICE_TYPE_MASTER)
		return;

	id = gdk_x11_device_get_id (gdk_device);
	device_file = xdevice_get_device_node (id);

	if (!device_file)
		return;

	/* Takes ownership of device_file */
	g_hash_table_insert (manager->gdk_devices, gdk_device, device_file);

	device = g_hash_table_lookup (manager->devices, device_file);

	if (device) {
		g_signal_emit_by_name (manager, "device-changed", device);
	} else {
		device = create_device (gdk_device, device_file);
		g_hash_table_insert (manager->devices, g_strdup (device_file), device);
		g_signal_emit_by_name (manager, "device-added", device);
	}
}

static void
remove_device (GsdX11DeviceManager *manager,
	       GdkDevice	   *gdk_device)
{
	const gchar *device_file;
	GsdDevice *device;

	device_file = g_hash_table_lookup (manager->gdk_devices, gdk_device);

	if (!device_file)
		return;

	device = g_hash_table_lookup (manager->devices, device_file);

	if (device)
		g_object_ref (device);

	if (device) {
		g_signal_emit_by_name (manager, "device-removed", device);
		g_object_unref (device);
	}

	g_hash_table_remove (manager->devices, device_file);
	g_hash_table_remove (manager->gdk_devices, gdk_device);
}

static void
init_devices (GsdX11DeviceManager *manager,
	      GdkDeviceType	   device_type)
{
	GList *devices, *l;

	devices = gdk_device_manager_list_devices (manager->device_manager,
						   device_type);

	for (l = devices; l; l = l->next)
		add_device (manager, l->data);

	g_list_free (devices);
}

static void
gsd_x11_device_manager_init (GsdX11DeviceManager *manager)
{
	GdkDisplay *display;

	manager->devices = g_hash_table_new_full (g_str_hash, g_str_equal,
						  (GDestroyNotify) g_free,
						  (GDestroyNotify) g_object_unref);
	manager->gdk_devices = g_hash_table_new_full (NULL, NULL, NULL,
						      (GDestroyNotify) g_free);

	display = gdk_display_get_default ();
	manager->device_manager = gdk_display_get_device_manager (display);

	g_signal_connect_swapped (manager->device_manager, "device-added",
				  G_CALLBACK (add_device), manager);
	g_signal_connect_swapped (manager->device_manager, "device-removed",
				  G_CALLBACK (remove_device), manager);

	init_devices (manager, GDK_DEVICE_TYPE_SLAVE);
	init_devices (manager, GDK_DEVICE_TYPE_FLOATING);
}

static GList *
gsd_x11_device_manager_list_devices (GsdDeviceManager *manager,
				     GsdDeviceType     type)
{
	GsdX11DeviceManager *manager_x11 = GSD_X11_DEVICE_MANAGER (manager);
	GsdDeviceType device_type;
	GList *devices = NULL;
	GHashTableIter iter;
	GsdDevice *device;

	g_hash_table_iter_init (&iter, manager_x11->devices);

	while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &device)) {
		device_type = gsd_device_get_device_type (device);

		if ((device_type & type) == type)
			devices = g_list_prepend (devices, device);
	}

	return devices;
}

static void
gsd_x11_device_manager_class_init (GsdX11DeviceManagerClass *klass)
{
	GsdDeviceManagerClass *manager_class = GSD_DEVICE_MANAGER_CLASS (klass);

	manager_class->list_devices = gsd_x11_device_manager_list_devices;
	manager_class->lookup_device = gsd_x11_device_manager_lookup_gdk_device;
}

GsdDevice *
gsd_x11_device_manager_lookup_gdk_device (GsdDeviceManager *manager,
					  GdkDevice	   *gdk_device)
{
	GsdX11DeviceManager *manager_x11 = GSD_X11_DEVICE_MANAGER (manager);
	const gchar *device_node;

	device_node = g_hash_table_lookup (manager_x11->gdk_devices, gdk_device);

	if (!device_node)
		return NULL;

	return g_hash_table_lookup (manager_x11->devices, device_node);
}
